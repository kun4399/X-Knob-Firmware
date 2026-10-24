#pragma once
#include <stdint.h>
#include <string.h>

// The first status after every MQTT connection supplies the event high-water
// mark. Retained or duplicate events at/below it cannot replay an alert. Events
// before that baseline are intentionally silent. Only MQTT's task owns this.
class CodexEventGate {
public:
    void reconnect() { ready_ = false; boot_[0] = 0; sequence_ = 0; }
    void status(const char *boot, uint32_t sequence) {
        if (!boot[0] || strlen(boot) >= sizeof(boot_)) return;
        if (!ready_ || strcmp(boot_, boot) != 0) {
            strcpy(boot_, boot);
            sequence_ = sequence;
            ready_ = true;
        }
    }
    bool accept(const char *boot, uint32_t sequence) {
        if (!ready_ || strcmp(boot_, boot) != 0 || sequence <= sequence_) return false;
        sequence_ = sequence;
        return true;
    }
private:
    bool ready_ = false;
    char boot_[32] = {};
    uint32_t sequence_ = 0;
};
