#include "FeedbackManager.h"
#include <M5Cardputer.h>

namespace Vanguard {

// M5Cardputer speaker uses I2S via M5's Speaker class, not direct GPIO.
// GPIO 43/44 are reserved for IR (TX:44, RX:43).

FeedbackManager& FeedbackManager::getInstance() {
    static FeedbackManager instance;
    return instance;
}

FeedbackManager::FeedbackManager() {}

void FeedbackManager::init() {
    // M5Cardputer.begin() already handles buzzer init usually
}

void FeedbackManager::beep(uint32_t freq, uint32_t duration) {
    if (!m_enabled) return;
    M5Cardputer.Speaker.tone(freq, duration);
}

void FeedbackManager::pulse(uint32_t duration) {
    if (!m_enabled) return;
    // Low frequency tone feels like vibration
    M5Cardputer.Speaker.tone(100, duration);
}

void FeedbackManager::updateGeiger(int8_t rssi) {
    if (!m_enabled || rssi == 0) return;

    uint32_t now = millis();
    
    // Map RSSI (-90 to -30) to interval (1000ms to 50ms)
    int8_t constrainedRssi = constrain(rssi, -90, -30);
    uint32_t interval = map(constrainedRssi, -90, -30, 1000, 50);

    if (now - m_lastGeigerMs >= interval) {
        // High pitch short click
        M5Cardputer.Speaker.tone(3000, 5); 
        m_lastGeigerMs = now;
    }
}

} // namespace Vanguard
