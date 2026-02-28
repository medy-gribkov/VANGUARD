#ifndef RADIO_WARDEN_H
#define RADIO_WARDEN_H

#include <Arduino.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>

namespace Vanguard {

enum class RadioOwner {
    NONE,
    OWNER_WIFI_STA,
    OWNER_WIFI_PROMISCUOUS,
    OWNER_BLE,
    OWNER_LORA // Future proofing
};

/**
 * @brief Singleton that arbitrates access to the shared ESP32-S3 radio.
 * 
 * Prevents "radio ripping" by ensuring clean handovers between protocols.
 */
class RadioWarden {
public:
    static RadioWarden& getInstance();

    /**
     * @brief Request the radio for a specific protocol.
     * @return true if access granted and radio initialized for that mode.
     */
    bool requestRadio(RadioOwner owner);

    /**
     * @brief Release the radio, putting hardware in a low-power/idle state.
     */
    void releaseRadio();

    /**
     * @brief Get current radio owner
     */
    RadioOwner getOwner() const { return m_currentOwner; }

private:
    RadioWarden();
    SemaphoreHandle_t m_radioMutex;
    RadioOwner m_currentOwner = RadioOwner::NONE;

    bool initWiFiSTA();
    bool initWiFiPromiscuous();
    bool initBLE();
    void shutdownCurrent();
};

} // namespace Vanguard

#endif // RADIO_WARDEN_H
