#ifndef VANGUARD_TYPES_H
#define VANGUARD_TYPES_H

/**
 * @file VanguardTypes.h
 * @brief Core data types for VANGUARD
 *
 * Defines all shared enums, structs, and type aliases used across
 * the Engine, UI, and Adapter layers.
 */

#include <Arduino.h>
#include <cstdint>

namespace Vanguard {

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
    BLE_SKIMMER,      // Detected Skimmer
    RF_DEVICE,        // Sub-GHz device (if supported)
    IR_DEVICE         // Infrared device
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
    TRANSITIONING_TO_BLE,  // Non-blocking WiFi→BLE transition
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
 * @brief WIDS Alert Types
 */
enum class WidsEventType : uint8_t {
    NONE,
    DEAUTH_FLOOD,
    EAPOL_FLOOD,
    KRACK_ATTACK,     // Future
    EVIL_TWIN_DETECT  // Future
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
    CANCELLED,
    STOPPED
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
// WIFI FRAME STRUCTURES (802.11)
// =============================================================================

/**
 * @brief Simplified 802.11 Frame Control field
 */
struct wifi_frame_control_t {
    uint16_t protocol : 2;
    uint16_t type : 2;
    uint16_t subtype : 4;
    uint16_t to_ds : 1;
    uint16_t from_ds : 1;
    uint16_t more_frag : 1;
    uint16_t retry : 1;
    uint16_t pwr_mgmt : 1;
    uint16_t more_data : 1;
    uint16_t protected_frame : 1;
    uint16_t order : 1;
};

/**
 * @brief Basic 802.11 Data Frame Header (3-address)
 */
struct wifi_data_frame_header_t {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t  addr1[6]; // RA / Destination
    uint8_t  addr2[6]; // TA / Source (Client if FromDS=0)
    uint8_t  addr3[6]; // BSSID (if FromDS=0)
    uint16_t seq_ctrl;
};

// Frame types
constexpr uint8_t WIFI_TYPE_MGMT = 0x00;
constexpr uint8_t WIFI_TYPE_DATA = 0x02;

// Subtypes
constexpr uint8_t WIFI_SUBTYPE_DATA = 0x00;
constexpr uint8_t WIFI_SUBTYPE_QOS_DATA = 0x08;

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
    uint8_t      clientMacs[MAX_CLIENTS_PER_AP][6]; // Track specific clients
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

    /**
     * @brief Check if a client is already tracked
     */
    bool hasClient(const uint8_t* mac) const {
        for (uint8_t i = 0; i < clientCount && i < MAX_CLIENTS_PER_AP; i++) {
            if (memcmp(clientMacs[i], mac, 6) == 0) return true;
        }
        return false;
    }

    /**
     * @brief Add a client to the list if not already present
     */
    bool addClientMac(const uint8_t* mac) {
        if (hasClient(mac)) return false;
        if (clientCount < MAX_CLIENTS_PER_AP) {
            memcpy(clientMacs[clientCount], mac, 6);
            clientCount++;
            return true;
        }
        return false;
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
    char         statusText[32]; // Fixed buffer, no dangling pointers
};

} // namespace Vanguard

#endif // VANGUARD_TYPES_H
