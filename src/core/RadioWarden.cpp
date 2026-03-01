#include "RadioWarden.h"
#include <WiFi.h>
#include "../adapters/BruceBLE.h"

namespace Vanguard {

RadioWarden& RadioWarden::getInstance() {
    static RadioWarden instance;
    return instance;
}

RadioWarden::RadioWarden() {
    m_radioMutex = xSemaphoreCreateMutex();
}

bool RadioWarden::requestRadio(RadioOwner owner) {
    if (m_currentOwner == owner) return true;

    if (xSemaphoreTake(m_radioMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        if (Serial) Serial.println("[Warden] Failed to acquire radio mutex");
        return false;
    }

    if (Serial) {
        Serial.printf("[Warden] Requesting handover: %d -> %d\n", (int)m_currentOwner, (int)owner);
    }

    shutdownCurrent();

    bool success = false;
    switch (owner) {
        case RadioOwner::OWNER_WIFI_STA:
            success = initWiFiSTA();
            break;
        case RadioOwner::OWNER_WIFI_PROMISCUOUS:
            success = initWiFiPromiscuous();
            break;
        case RadioOwner::OWNER_BLE:
            success = initBLE();
            break;
        default:
            success = true; // NONE
            break;
    }

    if (success) {
        m_currentOwner = owner;
    }
    xSemaphoreGive(m_radioMutex);
    return success;
}

void RadioWarden::releaseRadio() {
    if (xSemaphoreTake(m_radioMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        shutdownCurrent();
        m_currentOwner = RadioOwner::NONE;
        xSemaphoreGive(m_radioMutex);
    } else {
        // Force release even without mutex (better than deadlock)
        m_currentOwner = RadioOwner::NONE;
        if (Serial) Serial.println("[Warden] releaseRadio mutex timeout, forced release");
    }
}

void RadioWarden::shutdownCurrent() {
    if (m_currentOwner == RadioOwner::NONE) return;

    if (Serial) Serial.println("[Warden] Shutting down current radio...");

    // WiFi Cleanup
    if (m_currentOwner == RadioOwner::OWNER_WIFI_STA || m_currentOwner == RadioOwner::OWNER_WIFI_PROMISCUOUS) {
        esp_wifi_set_promiscuous(false);
        ::WiFi.disconnect(true);
        ::WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        // Give hardware time to settle (yield to scheduler, don't busy-wait)
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // BLE Cleanup - we don't deinit NimBLE, just stop activities
    if (m_currentOwner == RadioOwner::OWNER_BLE) {
        BruceBLE::getInstance().stopHardwareActivities();
    }
}

bool RadioWarden::initWiFiSTA() {
    ::WiFi.mode(WIFI_MODE_STA);
    if (esp_wifi_start() != ESP_OK) return false;
    return true;
}

bool RadioWarden::initWiFiPromiscuous() {
    ::WiFi.mode(WIFI_MODE_STA);
    if (esp_wifi_start() != ESP_OK) return false;
    esp_wifi_set_promiscuous(true);
    return true;
}

bool RadioWarden::initBLE() {
    // NimBLE init is handled by BruceBLE (should be init-once)
    return BruceBLE::getInstance().init();
}

} // namespace Vanguard
