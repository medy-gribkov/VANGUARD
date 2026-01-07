#ifndef ASSESSOR_TYPES_H
#define ASSESSOR_TYPES_H

/**
 * @file Types.h
 * @brief Core data types for The Assessor
 *
 * Defines all shared enums, structs, and type aliases used across
 * the Engine, UI, and Adapter layers.
 */

#include <Arduino.h>
#include <cstdint>

namespace Assessor {

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr size_t   MAX_TARGETS          = 64;
constexpr size_t   SSID_MAX_LEN         = 32;  // Renamed to avoid ESP-IDF conflict
constexpr size_t   MAX_CLIENTS_PER_AP   = 16;
constexpr uint8_t  WIFI_CHANNEL_MIN     = 1;
constexpr uint8_t  WIFI_CHANNEL_MAX     = 14;
constexpr uint32_t SCAN_TIMEOUT_MS      = 15000;
constexpr uint32_t TARGET_AGE_TIMEOUT   = 60000;  // Remove if not seen for 60s

// Signal strength thresholds (dBm)
constexpr int8_t   RSSI_EXCELLENT       = -50;
constexpr int8_t   RSSI_GOOD            = -60;
constexpr int8_t   RSSI_FAIR            = -70;
constexpr int8_t   RSSI_WEAK            = -80;

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief Type of discovered target
 */
enum class TargetType : uint8_t {
    UNKNOWN,
    ACCESS_POINT,     // WiFi Access Point (renamed to avoid ESP-IDF conflict)
    STATION,          // WiFi Client device
    BLE_DEVICE,       // Bluetooth LE device
    BLE_BEACON,       // iBeacon/Eddystone
    RF_DEVICE         // Sub-GHz device (if supported)
};

/**
 * @brief Security configuration of WiFi target
 */
enum class SecurityType : uint8_t {
    OPEN,
    WEP,
    WPA_PSK,
    WPA2_PSK,
    WPA2_ENTERPRISE,
    WPA3_SAE,
    UNKNOWN
};

/**
 * @brief Current state of the scanning subsystem
 */
enum class ScanState : uint8_t {
    IDLE,
    WIFI_SCANNING,
    BLE_SCANNING,
    COMPLETE,
    ERROR
};

/**
 * @brief Types of attacks available (mirrors Bruce capabilities)
 */
enum class ActionType : uint8_t {
    // Reconnaissance
    MONITOR,
    CAPTURE_HANDSHAKE,
    CAPTURE_PMKID,

    // WiFi Attacks
    DEAUTH_ALL,
    DEAUTH_SINGLE,
    BEACON_FLOOD,
    EVIL_TWIN,
    PROBE_FLOOD,

    // BLE Attacks
    BLE_SPAM,
    BLE_SOUR_APPLE,
    BLE_SKIMMER_DETECT,

    // IR (if equipped)
    IR_REPLAY,
    IR_TVBGONE,

    // RF (if equipped)
    RF_REPLAY,
    RF_JAM,

    // Meta
    NONE
};

/**
 * @brief Result of attempting an action
 */
enum class ActionResult : uint8_t {
    SUCCESS,
    IN_PROGRESS,
    FAILED_NO_TARGET,
    FAILED_NO_CLIENTS,
    FAILED_NOT_SUPPORTED,
    FAILED_HARDWARE,
    FAILED_TIMEOUT,
    CANCELLED
};

/**
 * @brief Signal strength classification
 */
enum class SignalStrength : uint8_t {
    EXCELLENT,   // > -50 dBm
    GOOD,        // -50 to -60
    FAIR,        // -60 to -70
    WEAK,        // -70 to -80
    POOR         // < -80
};

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Represents a discovered network target
 *
 * This is the core data structure that flows through the entire system.
 * The Radar displays it, the Engine tracks it, and Actions operate on it.
 */
struct Target {
    // Identity
    uint8_t      bssid[6];                   // MAC address
    char         ssid[SSID_MAX_LEN + 1];     // Network name (null-terminated)
    TargetType   type;

    // RF characteristics
    uint8_t      channel;
    int8_t       rssi;                       // Signal strength in dBm

    // Security (WiFi only)
    SecurityType security;

    // State (for context-aware actions)
    uint8_t      clientCount;                // Connected clients (APs only)
    bool         isHidden;                   // Hidden SSID
    bool         hasHandshake;               // We captured a handshake

    // Metadata
    uint32_t     firstSeenMs;
    uint32_t     lastSeenMs;
    uint16_t     beaconCount;                // Beacons/probes seen

    // ----- Helper Methods -----

    bool hasClients() const {
        return clientCount > 0;
    }

    bool isOpen() const {
        return security == SecurityType::OPEN;
    }

    SignalStrength getSignalStrength() const {
        if (rssi > RSSI_EXCELLENT) return SignalStrength::EXCELLENT;
        if (rssi > RSSI_GOOD)      return SignalStrength::GOOD;
        if (rssi > RSSI_FAIR)      return SignalStrength::FAIR;
        if (rssi > RSSI_WEAK)      return SignalStrength::WEAK;
        return SignalStrength::POOR;
    }

    bool isStale(uint32_t now) const {
        return (now - lastSeenMs) > TARGET_AGE_TIMEOUT;
    }

    /**
     * @brief Format BSSID as string (AA:BB:CC:DD:EE:FF)
     * @param buf Output buffer (at least 18 bytes)
     */
    void formatBssid(char* buf) const {
        snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                 bssid[0], bssid[1], bssid[2],
                 bssid[3], bssid[4], bssid[5]);
    }
};

/**
 * @brief Describes an available action for a target
 *
 * Generated by ActionResolver based on target state.
 */
struct AvailableAction {
    ActionType   type;
    const char*  label;        // Human-readable name
    const char*  description;  // One-line explanation
    bool         isDestructive; // Requires confirmation?
    bool         requiresClients; // Only valid if target has clients
};

/**
 * @brief Progress/status of an ongoing action
 */
struct ActionProgress {
    ActionType   type;
    ActionResult result;
    uint32_t     startTimeMs;
    uint32_t     elapsedMs;
    uint32_t     packetsSent;  // For attacks that send packets
    const char*  statusText;   // "Sending deauth frames..."
};

} // namespace Assessor

#endif // ASSESSOR_TYPES_H
