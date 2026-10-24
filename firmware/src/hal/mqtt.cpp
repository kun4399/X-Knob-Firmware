#include "hal/hal.h"

#if XK_MQTT

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include "hal/nvs.h"
#include "services/CodexEventGate.h"

namespace {
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
TaskHandle_t mqtt_task = nullptr;
QueueHandle_t outgoing = nullptr;
SemaphoreHandle_t state_lock = nullptr;
volatile bool broker_online = false;
bool mqtt_enabled = false;
HAL::SmartledState light_state = {};
HAL::CodexState codex_snapshot = {};
char broker_host[128];
char status_topic[128];
char command_topic[128];
constexpr char V1_LIGHT_SET[] = "xknob/v1/light/main/set";
constexpr char V1_LIGHT_STATE[] = "xknob/v1/light/main/state";
constexpr char V1_LIGHT_ACK[] = "xknob/v1/light/main/ack";
constexpr char V1_LIGHT_AVAIL[] = "xknob/v1/light/main/availability";
char V1_CODEX_STATUS[128];
char V1_CODEX_EVENT[128];
char V1_CODEX_USAGE[128];
char V1_CODEX_AVAIL[128];
char last_event_id[80] = {};
CodexEventGate codex_events;
portMUX_TYPE command_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t command_boot = 0;
uint32_t command_seq = 0;
String broker_user, broker_password, wifi_ssid, wifi_password;

struct Outgoing {
    char topic[128];
    char payload[384];
};

bool queue_publish(const char *topic, const char *payload)
{
    // UI tasks only enqueue small messages; PubSubClient stays owned by mqtt_worker.
    if (!broker_online || !outgoing || !topic || !payload || strlen(topic) >= sizeof(Outgoing::topic) ||
        strlen(payload) >= sizeof(Outgoing::payload)) return false;
    Outgoing item = {};
    strlcpy(item.topic, topic, sizeof(item.topic));
    strlcpy(item.payload, payload, sizeof(item.payload));
    return xQueueSend(outgoing, &item, 0) == pdTRUE;
}

void drop_connection() {
    broker_online = false;
    xQueueReset(outgoing);
    codex_events.reconnect();
    if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        light_state.online = false;
        light_state.received_ms = 0;
        light_state.last_ack_id[0] = 0;
        codex_snapshot.source_online = false;
        codex_snapshot.snapshot_valid = false;
        xSemaphoreGive(state_lock);
    }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    // MQTT owns parsing; UI only reads locked, bounded snapshots.
    if (length > 768) return;
    if (strcmp(topic, V1_LIGHT_AVAIL) == 0 || strcmp(topic, V1_CODEX_AVAIL) == 0) {
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            bool online = length == 6 && memcmp(payload, "online", 6) == 0;
            if (strcmp(topic, V1_LIGHT_AVAIL) == 0 && !online) light_state.online = false;
            if (strcmp(topic, V1_CODEX_AVAIL) == 0) codex_snapshot.source_online = online;
            xSemaphoreGive(state_lock);
        }
        return;
    }
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, payload, length)) return;
    if (strcmp(topic, V1_CODEX_STATUS) == 0) {
        const char *phase = doc["focused_phase"] | "unknown";
        const char *quality = doc["quality"] | "unknown";
        if (strlen(phase) >= sizeof(codex_snapshot.phase) ||
            strlen(quality) >= sizeof(codex_snapshot.quality)) return;
        codex_events.status(doc["boot_id"] | "", doc["event_seq"] | 0U);
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            strlcpy(codex_snapshot.phase, phase, sizeof(codex_snapshot.phase));
            strlcpy(codex_snapshot.quality, quality, sizeof(codex_snapshot.quality));
            codex_snapshot.running_count = doc["running_count"] | 0;
            codex_snapshot.approval_count = doc["approval_count"] | 0;
            codex_snapshot.session_count = 0;
            JsonArrayConst sessions = doc["sessions"].as<JsonArrayConst>();
            for (JsonObjectConst session : sessions) {
                if (codex_snapshot.session_count >= 4) break;
                const char *session_phase = session["phase"] | "unknown";
                if (strlen(session_phase) >= sizeof(codex_snapshot.session_phase[0])) continue;
                const uint8_t index = codex_snapshot.session_count++;
                strlcpy(codex_snapshot.session_phase[index], session_phase,
                        sizeof(codex_snapshot.session_phase[index]));
                const float age = session["age_s"] | 0.0f;
                codex_snapshot.session_age_seconds[index] = (uint16_t)constrain((int)age, 0, 65535);
            }
            codex_snapshot.snapshot_valid = true;
            codex_snapshot.received_ms = millis();
            xSemaphoreGive(state_lock);
        }
        return;
    }
    if (strcmp(topic, V1_CODEX_EVENT) == 0) {
        const char *id = doc["event_id"] | "";
        const char *kind = doc["kind"] | "";
        if (!id[0] || strlen(id) >= sizeof(last_event_id) ||
            strlen(kind) >= sizeof(codex_snapshot.event_kind)) return;
        if (!codex_events.accept(doc["boot_id"] | "", doc["seq"] | 0U)) return;
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (strcmp(last_event_id, id) != 0) {
                strlcpy(last_event_id, id, sizeof(last_event_id));
                strlcpy(codex_snapshot.event_kind, kind, sizeof(codex_snapshot.event_kind));
                ++codex_snapshot.event_revision;
            }
            xSemaphoreGive(state_lock);
        }
        return;
    }
    if (strcmp(topic, V1_CODEX_USAGE) == 0) {
        bool observed = strcmp(doc["quality"] | "", "observed") == 0;
        JsonArrayConst windows = doc["rate_limits"][0]["windows"].as<JsonArrayConst>();
        JsonVariantConst tokens = doc["token_activity"]["lifetime_tokens"];
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            codex_snapshot.usage_window_count = 0;
            for (JsonObjectConst window : windows) {
                if (codex_snapshot.usage_window_count >= 2) break;
                JsonVariantConst remaining = window["remaining_percent"];
                if (!remaining.is<float>() && !remaining.is<double>() && !remaining.is<int>()) continue;
                const uint8_t index = codex_snapshot.usage_window_count++;
                codex_snapshot.window_remaining_percent[index] = constrain(remaining.as<int>(), 0, 100);
                codex_snapshot.window_duration_minutes[index] =
                    constrain(window["duration_min"] | 0, 0, 100000);
            }
            codex_snapshot.usage_valid = observed && codex_snapshot.usage_window_count > 0;
            codex_snapshot.remaining_percent = codex_snapshot.usage_valid ?
                codex_snapshot.window_remaining_percent[0] : 0;
            codex_snapshot.lifetime_tokens =
                (tokens.is<uint64_t>() || tokens.is<unsigned long>() || tokens.is<int>()) ?
                tokens.as<uint64_t>() : 0;
            codex_snapshot.daily_token_count = 0;
            JsonArrayConst daily = doc["token_activity"]["daily_tokens"].as<JsonArrayConst>();
            for (JsonVariantConst amount : daily) {
                if (codex_snapshot.daily_token_count >= 7) break;
                codex_snapshot.daily_tokens[codex_snapshot.daily_token_count++] =
                    (amount.is<uint64_t>() || amount.is<unsigned long>() || amount.is<int>()) ?
                    amount.as<uint64_t>() : 0;
            }
            xSemaphoreGive(state_lock);
        }
        return;
    }
    if (strcmp(topic, V1_LIGHT_ACK) == 0) {
        const char *id = doc["command_id"] | "";
        if (!id[0] || strlen(id) >= sizeof(light_state.last_ack_id)) return;
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            strlcpy(light_state.last_ack_id, id, sizeof(light_state.last_ack_id));
            light_state.last_ack_success = doc["success"] | false;
            light_state.ack_received_ms = millis();
            xSemaphoreGive(state_lock);
        }
        return;
    }
    if (strcmp(topic, V1_LIGHT_STATE) == 0) {
        HAL::SmartledState next = {};
        next.modern = true;
        next.online = doc["online"] | false;
        next.ok = doc["ok"] | false;
        if (next.online) {
            next.power = doc["power"] | false;
            next.brightness = doc["brightness_percent"] | 0;
            next.temperature = doc["color_temperature_kelvin"] | 0;
            if (next.brightness < 1 || next.brightness > 100 ||
                next.temperature < 2700 || next.temperature > 6500) return;
        }
        strlcpy(next.last_command_id, doc["last_command_id"] | "", sizeof(next.last_command_id));
        next.received_ms = millis();
        if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
            strlcpy(next.last_ack_id, light_state.last_ack_id, sizeof(next.last_ack_id));
            next.last_ack_success = light_state.last_ack_success;
            next.ack_received_ms = light_state.ack_received_ms;
            light_state = next;
            xSemaphoreGive(state_lock);
        }
        return;
    }
    if (strcmp(topic, status_topic) != 0) return;
    if (!doc["online"].is<bool>() || !doc["ok"].is<bool>()) return;
    HAL::SmartledState next = {};
    next.online = doc["online"].as<bool>();
    next.ok = doc["ok"].as<bool>();
    if (next.online) {
        if (!doc["power"].is<bool>() || !doc["brightness"].is<int>() ||
            !doc["temperature"].is<int>()) return;
        next.power = doc["power"].as<bool>();
        next.brightness = doc["brightness"].as<int>();
        next.temperature = doc["temperature"].as<int>();
        if (next.brightness < 1 || next.brightness > 100 ||
            next.temperature < 2700 || next.temperature > 6500) return;
    }
    next.received_ms = millis();
    if (xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        // A rolled-back gateway only publishes the legacy topic. Let it take over
        // once V1 readback has gone stale so the migration remains reversible.
        if (!light_state.modern || millis() - light_state.received_ms > 90000)
            light_state = next;
        xSemaphoreGive(state_lock);
    }
}

void mqtt_worker(void *)
{
    unsigned long last_attempt = 0;
    unsigned long last_wifi_attempt = 0;
    bool wifi_begin_issued = false;
    bool modem_sleep_set = false;
    for (;;) {
        unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            modem_sleep_set = false;
            if (mqtt_enabled) {
                drop_connection();
            }
            // WiFi.begin() tears down/reinitializes the driver. Repeating it
            // while the STA is still scanning can exhaust RX buffers and emit
            // `Failed to deinit Wi-Fi`; let the STA auto-reconnect instead.
            if (!wifi_begin_issued && now - last_wifi_attempt > 5000) {
                last_wifi_attempt = now;
                WiFi.setAutoReconnect(true);
                WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
                wifi_begin_issued = true;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!modem_sleep_set) {
            // MIN_MODEM keeps the connection while reducing RF duty cycle;
            // MAX_MODEM risks extra latency or dropped broadcast frames.
            modem_sleep_set = WiFi.setSleep(true);
        }
        if (!mqtt_enabled) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (!mqtt_client.connected()) {
            drop_connection();
            if (now - last_attempt > 5000) {
                last_attempt = now;
                String client_id = String("xknob-") + String((uint32_t)ESP.getEfuseMac(), HEX);
                bool connected = broker_user.length() ?
                    mqtt_client.connect(client_id.c_str(), broker_user.c_str(), broker_password.c_str()) :
                    mqtt_client.connect(client_id.c_str());
                if (connected) {
                    broker_online = true;
                    mqtt_client.subscribe(status_topic, 1);
                    mqtt_client.subscribe(V1_LIGHT_STATE, 1);
                    mqtt_client.subscribe(V1_LIGHT_ACK, 1);
                    mqtt_client.subscribe(V1_LIGHT_AVAIL, 1);
                    mqtt_client.subscribe(V1_CODEX_STATUS, 1);
                    mqtt_client.subscribe(V1_CODEX_EVENT, 1);
                    mqtt_client.subscribe(V1_CODEX_USAGE, 1);
                    mqtt_client.subscribe(V1_CODEX_AVAIL, 1);
                    mqtt_client.publish(command_topic, "state", false);
                }
            }
        } else {
            mqtt_client.loop();
            if (!mqtt_client.connected()) {
                drop_connection();
                continue;
            }
            Outgoing item;
            while (xQueueReceive(outgoing, &item, 0) == pdTRUE) {
                if (!mqtt_client.publish(item.topic, item.payload, false)) {
                    drop_connection();
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

void HAL::mqtt_init(void)
{
    if (mqtt_task) return;
    String host, topic;
    uint16_t port;
    get_wifi_config(wifi_ssid, wifi_password);
    get_mqtt_config(host, port, broker_user, broker_password, topic);
    if (wifi_ssid.length() == 0) return;
    state_lock = xSemaphoreCreateMutex();
    const int status_len = snprintf(V1_CODEX_STATUS, sizeof(V1_CODEX_STATUS),
                                    "xknob/v1/codex/%s/status", XK_CODEX_HOST_ID);
    const int event_len = snprintf(V1_CODEX_EVENT, sizeof(V1_CODEX_EVENT),
                                  "xknob/v1/codex/%s/event", XK_CODEX_HOST_ID);
    const int usage_len = snprintf(V1_CODEX_USAGE, sizeof(V1_CODEX_USAGE),
                                   "xknob/v1/codex/%s/usage", XK_CODEX_HOST_ID);
    const int availability_len = snprintf(V1_CODEX_AVAIL, sizeof(V1_CODEX_AVAIL),
                                          "xknob/v1/codex/%s/availability", XK_CODEX_HOST_ID);
    const bool codex_topics_valid = status_len > 0 && status_len < sizeof(V1_CODEX_STATUS) &&
        event_len > 0 && event_len < sizeof(V1_CODEX_EVENT) &&
        usage_len > 0 && usage_len < sizeof(V1_CODEX_USAGE) &&
        availability_len > 0 && availability_len < sizeof(V1_CODEX_AVAIL);
    mqtt_enabled = host.length() > 0 && topic.length() > 0 && port > 0 &&
                   host.length() < sizeof(broker_host) &&
                   codex_topics_valid &&
                   snprintf(command_topic, sizeof(command_topic), "%s/smartled/strip/set", topic.c_str()) < sizeof(command_topic) &&
                   snprintf(status_topic, sizeof(status_topic), "%s/smartled/strip/state", topic.c_str()) < sizeof(status_topic);
    if (mqtt_enabled) {
        strlcpy(broker_host, host.c_str(), sizeof(broker_host));
        outgoing = xQueueCreate(12, sizeof(Outgoing));
        if (!outgoing || !state_lock) {
            if (outgoing) { vQueueDelete(outgoing); outgoing = nullptr; }
            mqtt_enabled = false;
        }
    }
    if (mqtt_enabled) {
        command_boot = esp_random();
        mqtt_client.setServer(broker_host, port);
        mqtt_client.setBufferSize(1024);
        mqtt_client.setKeepAlive(60);
        mqtt_client.setSocketTimeout(2);
        mqtt_client.setCallback(mqtt_callback);
    }
    xTaskCreatePinnedToCore(mqtt_worker, "MqttThread", 6144, nullptr, 2,
                            &mqtt_task, ESP32_RUNNING_CORE);
}

int HAL::mqtt_publish(const char *topic, const char *payload)
{
    return queue_publish(topic, payload) ? 1 : 0;
}

bool HAL::smartled_command(const char *command)
{
    return broker_online && command_topic[0] && queue_publish(command_topic, command);
}

bool HAL::smartled_set(bool include_power, bool power, bool include_brightness,
                       int brightness, bool include_temperature, int temperature,
                       char *command_id, size_t command_id_size)
{
    if (!broker_online || !command_id || command_id_size < 24 ||
        (!include_power && !include_brightness && !include_temperature) ||
        (include_brightness && (brightness < 1 || brightness > 100)) ||
        (include_temperature && (temperature < 2700 || temperature > 6500))) return false;
    char id[64];
    portENTER_CRITICAL(&command_mux);
    const uint32_t sequence = ++command_seq;
    portEXIT_CRITICAL(&command_mux);
    snprintf(id, sizeof(id), "k%08lx-%lu", (unsigned long)command_boot,
             (unsigned long)sequence);
    bool modern = false;
    if (state_lock && xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        modern = light_state.modern;
        xSemaphoreGive(state_lock);
    }
    if (!modern) {
        // The WROOM-32 may still run the legacy gateway during the migration.
        // These are absolute writes; the UI confirms only a matching BLE readback.
        char command[40];
        if (include_brightness) {
            snprintf(command, sizeof(command), "brightness %d", brightness);
            if (!queue_publish(command_topic, command)) return false;
        }
        if (include_temperature) {
            snprintf(command, sizeof(command), "temperature %d", temperature);
            if (!queue_publish(command_topic, command)) return false;
        }
        if (include_power) {
            snprintf(command, sizeof(command), "power %d", power ? 1 : 0);
            if (!queue_publish(command_topic, command)) return false;
        }
        strlcpy(command_id, id, command_id_size);
        return true;
    }
    StaticJsonDocument<384> doc;
    doc["v"] = 1;
    doc["command_id"] = id;
    JsonObject desired = doc.createNestedObject("desired");
    if (include_power) desired["power"] = power;
    if (include_brightness) desired["brightness_percent"] = brightness;
    if (include_temperature) desired["color_temperature_kelvin"] = temperature;
    char body[384];
    if (serializeJson(doc, body, sizeof(body)) == 0 || !queue_publish(V1_LIGHT_SET, body)) return false;
    strlcpy(command_id, id, command_id_size);
    return true;
}

bool HAL::smartled_state(SmartledState *out)
{
    if (!out || !state_lock || xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    *out = light_state;
    xSemaphoreGive(state_lock);
    if (!broker_online || millis() - out->received_ms > 90000) out->online = false;
    return true;
}

bool HAL::codex_state(CodexState *out)
{
    if (!out || !state_lock || xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    *out = codex_snapshot;
    xSemaphoreGive(state_lock);
    if (!broker_online || millis() - out->received_ms > 240000) out->source_online = false;
    return true;
}

#else
void HAL::mqtt_init(void) {}
int HAL::mqtt_publish(const char *, const char *) { return 0; }
bool HAL::smartled_command(const char *) { return false; }
bool HAL::smartled_set(bool, bool, bool, int, bool, int, char *, size_t) { return false; }
bool HAL::smartled_state(SmartledState *) { return false; }
bool HAL::codex_state(CodexState *) { return false; }
#endif
