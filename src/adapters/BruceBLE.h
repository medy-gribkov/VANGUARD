#ifndef VANGUARD_BRUCE_BLE_H
#define VANGUARD_BRUCE_BLE_H

/**
 * @file BruceBLE.h
 * @brief Adapter layer wrapping Bruce's BLE attack functions
 *
 * This adapter provides access to Bruce's BLE capabilities:
 * - Device scanning
 * - BLE spam attacks (iOS, Windows, Samsung, Android)
 * - iBeacon spoofing
 * - Skimmer detection
 *
 * Bruce Functions Wrapped:
 * - aj_adv(choice) → spam()
 * - ibeacon() → spoofBeacon()
 *
 * @see BruceAnalysis.md for full mapping
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../core/VanguardTypes.h"
#include "../core/VanguardModule.h"
#include <functional>
#include <vector>

namespace Vanguard {

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr uint32_t BLE_SCAN_DURATION_MS = 5000;   // Default scan time
constexpr uint32_t BLE_ADV_INTERVAL_MS  = 20;     // Spam advertisement interval

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief BLE adapter operational state
 */
enum class BLEAdapterState : uint8_t {
    IDLE,
    SCANNING,
    SPAMMING,
    BEACON_SPOOFING,
    ERROR
};

/**
 * @brief BLE spam attack types (maps to Bruce's aj_adv choices)
 */
enum class BLESpamType : uint8_t {
    IOS_POPUP,          // iOS device pairing popups
    IOS_ACTION,         // iOS action notifications
    ANDROID_FAST_PAIR,  // Android Fast Pair spam
    WINDOWS_SWIFT_PAIR, // Windows Swift Pair spam
    SAMSUNG_BUDS,       // Samsung Galaxy Buds spam
    SOUR_APPLE,         // Apple device DoS
    RANDOM              // Random mix of above
};

/**
 * @brief Discovered BLE device info
 */
struct BLEDeviceInfo {
    uint8_t      address[6];     // BLE MAC address
    char         name[32];       // Device name (if advertised)
    int8_t       rssi;           // Signal strength
    uint16_t     appearance;     // Device appearance code
    bool         isConnectable;  // Can we connect?
    bool         hasServices;    // Advertises services?
    uint32_t     lastSeenMs;

    // Manufacturer data (for detection)
    uint16_t     manufacturerId;
    uint8_t      manufacturerData[32];
    uint8_t      manufacturerDataLen;
    bool         isSuspicious;   // Flagged by detection logic
};

// =============================================================================
// CALLBACKS
// =============================================================================

using BLEScanCallback = std::function<void(const BLEDeviceInfo&)>;
using BLEScanCompleteCallback = std::function<void(int deviceCount)>;
using BLESpamProgressCallback = std::function<void(uint32_t advertisementsSent)>;

// =============================================================================
// BruceBLE Adapter Class
// =============================================================================

class BruceBLE : public VanguardModule {
public:
    /**
     * @brief Get singleton instance
     */
    static BruceBLE& getInstance();

    // VanguardModule interface
    bool onEnable() override;
    void onDisable() override;
    void onTick() override;
    const char* getName() const override { return "BLE"; }


    // Prevent copying
    BruceBLE(const BruceBLE&) = delete;
    BruceBLE& operator=(const BruceBLE&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize BLE hardware
     * @return true if successful
     */
    bool init();

    /**
     * @brief Shutdown BLE cleanly
     */
    void shutdown();

    /**
     * @brief Non-blocking tick - MUST call every loop()
     */
    void tick();
    /**
     * @brief Get current adapter state
     */
    BLEAdapterState getState() const;

    // -------------------------------------------------------------------------
    // Scanning
    // -------------------------------------------------------------------------

    /**
     * @brief Start BLE device scan
     *
     * @param durationMs Scan duration (0 = continuous until stop)
     * @return true if scan started
     */
    bool beginScan(uint32_t durationMs = BLE_SCAN_DURATION_MS);

    /**
     * @brief Stop ongoing scan
     */
    void stopScan();

    /**
     * @brief Stop any active hardware operations (attacks, scans)
     */
    void stopHardwareActivities();



    /**
     * @brief Check if scan is complete
     */
    bool isScanComplete() const;

    /**
     * @brief Get discovered devices
     */
    const std::vector<BLEDeviceInfo>& getDevices() const;

    /**
     * @brief Get device count
     */
    size_t getDeviceCount() const;

    /**
     * @brief Register device found callback
     */
    void onDeviceFound(BLEScanCallback cb);

    /**
     * @brief Register scan complete callback
     */
    void onScanComplete(BLEScanCompleteCallback cb);

    // -------------------------------------------------------------------------
    // Spam Attacks
    // -------------------------------------------------------------------------

    /**
     * @brief Start BLE spam attack
     *
     * Wraps Bruce's aj_adv().
     * Floods nearby devices with fake advertisements.
     *
     * @param type Type of spam to send
     * @return true if attack started
     */
    bool startSpam(BLESpamType type);

    /**
     * @brief Stop spam attack
     */
    void stopSpam();

    /**
     * @brief Get advertisements sent
     */
    uint32_t getAdvertisementsSent() const;

    /**
     * @brief Register spam progress callback
     */
    void onSpamProgress(BLESpamProgressCallback cb);

    // -------------------------------------------------------------------------
    // iBeacon Spoofing
    // -------------------------------------------------------------------------

    /**
     * @brief Start broadcasting fake iBeacon
     *
     * Wraps Bruce's ibeacon().
     *
     * @param name Beacon name
     * @param uuid Beacon UUID
     * @param major Major version
     * @param minor Minor version
     * @return true if broadcasting started
     */
    bool spoofBeacon(const char* name,
                     const char* uuid,
                     uint16_t major = 0,
                     uint16_t minor = 0);

    /**
     * @brief Clone an existing beacon
     *
     * @param original Device to clone
     * @return true if cloning started
     */
    bool cloneBeacon(const BLEDeviceInfo& original);

    /**
     * @brief Stop beacon broadcast
     */
    void stopBeacon();

    // -------------------------------------------------------------------------
    // Detection
    // -------------------------------------------------------------------------

    /**
     * @brief Check if device looks like a skimmer
     *
     * Analyzes manufacturer data and characteristics.
     *
     * @param device Device to analyze
     * @return true if suspicious
     */
    bool isLikelySkimmer(const BLEDeviceInfo& device) const;

    /**
     * @brief Get list of suspicious devices from last scan
     */
    std::vector<BLEDeviceInfo> getSuspiciousDevices() const;



private:
    BruceBLE();
    ~BruceBLE();

    // State
    BLEAdapterState        m_state;
    bool                   m_initialized;

    // Scan results
    std::vector<BLEDeviceInfo> m_devices;
    uint32_t               m_scanStartMs;
    uint32_t               m_scanDurationMs;

    // Attack state
    BLESpamType            m_spamType;
    uint32_t               m_advertisementsSent;
    uint32_t               m_lastAdvMs;

    // NimBLE objects
    NimBLEScan*            m_scanner;
    NimBLEAdvertising*     m_advertising;

    // Callbacks
    BLEScanCallback        m_onDeviceFound;
    BLEScanCompleteCallback m_onScanComplete;
    BLESpamProgressCallback m_onSpamProgress;

    // Internal tick handlers
    void tickScan();
    void tickSpam();
    void tickBeacon();

    // Spam generators (from Bruce aj_adv)
    void generateIOSSpamData(uint8_t* data, size_t* len);
    void generateAndroidSpamData(uint8_t* data, size_t* len);
    void generateWindowsSpamData(uint8_t* data, size_t* len);
    void generateSamsungSpamData(uint8_t* data, size_t* len);
    void generateSourAppleData(uint8_t* data, size_t* len);

    // NimBLE scan callback
    class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    public:
        ScanCallbacks(BruceBLE* parent) : m_parent(parent) {}
        void onResult(NimBLEAdvertisedDevice* device) override;
    private:
        BruceBLE* m_parent;
    };

    ScanCallbacks* m_scanCallbacks;
};

} // namespace Vanguard

#endif // VANGUARD_BRUCE_BLE_H
