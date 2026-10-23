#include "hal/hal.h"

#if XK_MQTT

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "hal/nvs.h"

namespace {
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
TaskHandle_t mqtt_task = nullptr;
QueueHandle_t outgoing = nullptr;
SemaphoreHandle_t state_lock = nullptr;
volatile bool broker_online = false;
HAL::SmartledState light_state = {};
char broker_host[128];
char status_topic[128];
char command_topic[128];
String broker_user, broker_password, wifi_ssid, wifi_password;

struct Outgoing {
    char topic[128];
    char payload[64];
};

bool queue_publish(const char *topic, const char *payload)
{
    // UI tasks only enqueue small messages; PubSubClient stays owned by mqtt_worker.
    if (!outgoing || !topic || !payload || strlen(topic) >= sizeof(Outgoing::topic) ||
        strlen(payload) >= sizeof(Outgoing::payload)) return false;
    Outgoing item = {};
    strlcpy(item.topic, topic, sizeof(item.topic));
    strlcpy(item.payload, payload, sizeof(item.payload));
    return xQueueSend(outgoing, &item, 0) == pdTRUE;
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    // Reject malformed or out-of-range state before it reaches the UI.
    if (strcmp(topic, status_topic) != 0 || length > 256) return;
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload, length)) return;
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
        light_state = next;
        xSemaphoreGive(state_lock);
    }
}

void mqtt_worker(void *)
{
    unsigned long last_attempt = 0;
    unsigned long last_wifi_attempt = 0;
    for (;;) {
        unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            broker_online = false;
            xQueueReset(outgoing);
            if (now - last_wifi_attempt > 5000) {
                last_wifi_attempt = now;
                WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
            }
        } else if (!mqtt_client.connected()) {
            broker_online = false;
            xQueueReset(outgoing);
            if (now - last_attempt > 5000) {
                last_attempt = now;
                String client_id = String("xknob-") + String((uint32_t)ESP.getEfuseMac(), HEX);
                bool connected = broker_user.length() ?
                    mqtt_client.connect(client_id.c_str(), broker_user.c_str(), broker_password.c_str()) :
                    mqtt_client.connect(client_id.c_str());
                if (connected) {
                    broker_online = true;
                    mqtt_client.subscribe(status_topic, 1);
                    mqtt_client.publish(command_topic, "state", false);
                }
            }
        } else {
            mqtt_client.loop();
            Outgoing item;
            while (xQueueReceive(outgoing, &item, 0) == pdTRUE)
                mqtt_client.publish(item.topic, item.payload, false);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
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
    // Empty configuration is a valid offline state; the screen remains usable.
    if (wifi_ssid.length() == 0 || host.length() == 0 || topic.length() == 0 || port == 0) return;
    if (host.length() >= sizeof(broker_host) ||
        snprintf(command_topic, sizeof(command_topic), "%s/smartled/strip/set", topic.c_str()) >= sizeof(command_topic) ||
        snprintf(status_topic, sizeof(status_topic), "%s/smartled/strip/state", topic.c_str()) >= sizeof(status_topic)) return;
    strlcpy(broker_host, host.c_str(), sizeof(broker_host));
    outgoing = xQueueCreate(12, sizeof(Outgoing));
    state_lock = xSemaphoreCreateMutex();
    if (!outgoing || !state_lock) return;
    mqtt_client.setServer(broker_host, port);
    mqtt_client.setCallback(mqtt_callback);
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

bool HAL::smartled_state(SmartledState *out)
{
    if (!out || !state_lock || xSemaphoreTake(state_lock, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    *out = light_state;
    xSemaphoreGive(state_lock);
    if (!broker_online || millis() - out->received_ms > 30000) out->online = false;
    return true;
}

#else
void HAL::mqtt_init(void) {}
int HAL::mqtt_publish(const char *, const char *) { return 0; }
bool HAL::smartled_command(const char *) { return false; }
bool HAL::smartled_state(SmartledState *) { return false; }
#endif
