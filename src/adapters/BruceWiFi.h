#ifndef VANGUARD_BRUCE_WIFI_H
#define VANGUARD_BRUCE_WIFI_H

/**
 * @file BruceWiFi.h
 * @brief Adapter layer wrapping Bruce's WiFi attack functions
 *
 * This adapter provides a clean, async interface to Bruce's WiFi
 * capabilities. It converts Bruce's blocking functions into
 * non-blocking state machines suitable for our UI.
 *
 * Bruce Functions Wrapped:
 * - stationDeauth() → deauthStation()
 * - deauthFloodAttack() → deauthAll()
 * - beaconAttack() → beaconFlood()
 * - capture_handshake() → captureHandshake()
 * - EvilPortal class → startEvilTwin()
 * - sniffer functions → startMonitor()
 *
 * @see BruceAnalysis.md for full mapping
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "../core/VanguardTypes.h"
#include "../core/VanguardModule.h"
#include <functional>

namespace Vanguard {

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr uint32_t DEAUTH_INTERVAL_MS   = 10;    // Time between deauth frames
constexpr uint32_t BEACON_INTERVAL_MS   = 100;   // Time between beacon frames
constexpr uint32_t SCAN_POLL_INTERVAL   = 100;   // How often to check scan status
constexpr size_t   MAX_BEACON_SSIDS     = 32;    // For beacon flood attack

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief WiFi adapter operational state
 */
enum class WiFiAdapterState : uint8_t {
    IDLE,
    SCANNING,
    DEAUTHING,
    BEACON_FLOODING,
    CAPTURING_HANDSHAKE,
    EVIL_TWIN_ACTIVE,
    MONITORING,
    ERROR
};

/**
 * @brief Scan result from async WiFi scan
 */
struct ScanResultEntry {
    uint8_t      bssid[6];
    char         ssid[33];
    int8_t       rssi;
    uint8_t      channel;
    uint8_t      encryptionType;  // WIFI_AUTH_* constants
};

// =============================================================================
// CALLBACKS
// =============================================================================

using ScanCompleteCallback = std::function<void(int networkCount)>;
using AttackProgressCallback = std::function<void(uint32_t packetsSent)>;
using HandshakeCapturedCallback = std::function<void(const uint8_t* bssid)>;
using CredentialCapturedCallback = std::function<void(const char* ssid, const char* password)>;
using PacketCallback = std::function<void(const uint8_t* payload, uint16_t len, int8_t rssi)>;
using AssociationCallback = std::function<void(const uint8_t* clientMac, const uint8_t* apMac)>;

// =============================================================================
// BruceWiFi Adapter Class
// =============================================================================

class BruceWiFi : public VanguardModule {
public:
    /**
     * @brief Get singleton instance (one WiFi radio)
     */
    static BruceWiFi& getInstance();

    // VanguardModule interface
    bool onEnable() override;
    void onDisable() override;
    void onTick() override;
    const char* getName() const override { return "WiFi"; }


    // Prevent copying
    BruceWiFi(const BruceWiFi&) = delete;
    BruceWiFi& operator=(const BruceWiFi&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize WiFi hardware and promiscuous mode
     * @return true if successful
     */
    bool init();

    /**
     * @brief Shutdown WiFi cleanly
     */
    void shutdown();

    /**
     * @brief Non-blocking tick - MUST call every loop()
     */
    void tick();

    /**
     * @brief Get current adapter state
     */
    WiFiAdapterState getState() const;

    // -------------------------------------------------------------------------
    // Scanning (Async)
    // -------------------------------------------------------------------------

    /**
     * @brief Start async WiFi scan
     *
     * Wraps WiFi.scanNetworks(true) for non-blocking operation.
     * Check getScanState() or use callback.
     */
    void beginScan();

    /**
     * @brief Stop ongoing scan
     */
    void stopScan();

    /**
     * @brief Check if scan is complete
     */
    bool isScanComplete() const;

    /**
     * @brief Get number of networks found
     */
    int getScanResultCount() const;

    /**
     * @brief Get scan result by index
     * @param index 0-based index
     * @param result Output structure
     * @return true if valid index
     */
    bool getScanResult(int index, ScanResultEntry& result) const;

    /**
     * @brief Register scan complete callback
     */
    void onScanComplete(ScanCompleteCallback cb);

    // -------------------------------------------------------------------------
    // Deauthentication Attacks
    // -------------------------------------------------------------------------

    /**
     * @brief Deauth a single station from an AP
     *
     * Wraps Bruce's stationDeauth().
     * Non-blocking - sends frames over time.
     *
     * @param stationMac Client MAC to disconnect
     * @param apMac Access point MAC
     * @param channel WiFi channel
     * @return true if attack started
     */
    bool deauthStation(const uint8_t* stationMac,
                       const uint8_t* apMac,
                       uint8_t channel);

    /**
     * @brief Deauth all clients from an AP
     *
     * Wraps Bruce's deauthFloodAttack().
     * Sends broadcast deauth frames.
     *
     * @param apMac Access point MAC (BSSID)
     * @param channel WiFi channel
     * @return true if attack started
     */
    bool deauthAll(const uint8_t* apMac, uint8_t channel);

    // -------------------------------------------------------------------------
    // Beacon Attacks
    // -------------------------------------------------------------------------

    /**
     * @brief Start beacon flood attack
     *
     * Wraps Bruce's beaconAttack().
     * Spams fake AP beacons.
     *
     * @param ssids Array of SSID strings to broadcast
     * @param count Number of SSIDs
     * @param channel Channel to broadcast on
     * @return true if attack started
     */
    bool beaconFlood(const char** ssids, size_t count, uint8_t channel);

    /**
     * @brief Clone an existing AP's beacon
     *
     * @param ssid SSID to clone
     * @param bssid MAC to spoof (or nullptr for random)
     * @param channel Channel to broadcast on
     * @return true if attack started
     */
    bool cloneBeacon(const char* ssid,
                     const uint8_t* bssid,
                     uint8_t channel);

    // -------------------------------------------------------------------------
    // Handshake Capture
    // -------------------------------------------------------------------------

    /**
     * @brief Start capturing WPA handshake
     *
     * Wraps Bruce's capture_handshake() + sniffer.
     * Optionally sends deauth to force reconnection.
     *
     * @param apMac Target AP MAC
     * @param channel Target channel
     * @param sendDeauth Send deauth frames to speed capture
     * @return true if capture started
     */
    bool captureHandshake(const uint8_t* apMac,
                          uint8_t channel,
                          bool sendDeauth = true);

    /**
     * @brief Check if handshake was captured
     */
    bool hasHandshake() const;

    /**
     * @brief Save captured handshake to file
     * @param filename Path on SD card
     * @return true if saved successfully
     */
    bool saveHandshake(const char* filename);

    /**
     * @brief Register handshake capture callback
     */
    void onHandshakeCaptured(HandshakeCapturedCallback cb);

    // -------------------------------------------------------------------------
    // Evil Twin
    // -------------------------------------------------------------------------

    /**
     * @brief Start evil twin attack
     *
     * Wraps Bruce's EvilPortal class.
     * Creates fake AP with captive portal.
     *
     * @param ssid Network name to clone
     * @param channel Channel to operate on
     * @param sendDeauth Deauth real AP to force clients over
     * @return true if portal started
     */
    bool startEvilTwin(const char* ssid,
                       uint8_t channel,
                       bool sendDeauth = true);

    /**
     * @brief Stop evil twin and restore normal mode
     */
    void stopEvilTwin();

    /**
     * @brief Get captured credentials count
     */
    int getCapturedCredentialCount() const;

    /**
     * @brief Register credential capture callback
     */
    void onCredentialCaptured(CredentialCapturedCallback cb);

    // -------------------------------------------------------------------------
    // Passive Monitoring
    // -------------------------------------------------------------------------

    /**
     * @brief Start passive packet monitoring
     *
     * Wraps Bruce's sniffer_setup().
     * Captures all visible packets.
     *
     * @param channel Channel to monitor (0 = hop)
     * @return true if monitoring started
     */
    bool startMonitor(uint8_t channel = 0);

    /**
     * @brief Stop monitoring
     */
    void stopMonitor();

    /**
     * @brief Register packet callback
     */
    void onPacketReceived(PacketCallback cb);

    /**
     * @brief Register association callback (client discovery)
     */
    void onAssociation(AssociationCallback cb);

    // -------------------------------------------------------------------------
    // Attack Control (Common)
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable PCAP logging to SD card
     */
    void setPcapLogging(bool enabled, const char* filename = nullptr);

    /**
     * @brief Stop any active attack
     */
    void stopHardwareActivities();

    /**
     * @brief Get packets sent during current attack
     */
    uint32_t getPacketsSent() const;

    /**
     * @brief Get EAPOL frames captured during current attack
     */
    uint32_t getEapolCount() const { return m_eapolCount; }

    /**
     * @brief Register attack progress callback
     */
    void onAttackProgress(AttackProgressCallback cb);

    // -------------------------------------------------------------------------
    // Low-Level (Direct Bruce Access)
    // -------------------------------------------------------------------------

    /**
     * @brief Send raw 802.11 frame
     *
     * Direct wrapper for esp_wifi_80211_tx().
     *
     * @param frame Frame buffer
     * @param len Frame length
     * @return true if sent successfully
     */
    bool sendRawFrame(const uint8_t* frame, size_t len);

    /**
     * @brief Set WiFi channel
     */
    bool setChannel(uint8_t channel);

    /**
     * @brief Enable/disable promiscuous mode
     */
    bool setPromiscuous(bool enable);

private:
    BruceWiFi();
    ~BruceWiFi();

    // State
    WiFiAdapterState   m_state;
    bool               m_initialized;
    bool               m_promiscuousEnabled;
    uint8_t            m_currentChannel;

    // Attack state
    uint32_t           m_packetsSent;
    uint32_t           m_lastPacketMs;
    uint8_t            m_attackTargetMac[6];
    uint8_t            m_attackApMac[6];

    // Handshake capture
    bool               m_handshakeCaptured;

    // Callbacks
    ScanCompleteCallback      m_onScanComplete;
    AttackProgressCallback    m_onAttackProgress;
    HandshakeCapturedCallback m_onHandshakeCaptured;
    CredentialCapturedCallback m_onCredentialCaptured;
    PacketCallback            m_onPacketReceived;
    AssociationCallback       m_onAssociation;
    uint32_t                  m_eapolCount;

    // Beacon flood state (moved from file-scope statics)
    const char**              m_beaconSsids;
    size_t                    m_beaconSsidCount;
    size_t                    m_beaconCurrentIndex;
    uint8_t                   m_beaconChannel;

    // Evil twin state (moved from file-scope statics)
    char                      m_evilTwinSSID[33];
    uint8_t                   m_evilTwinChannel;
    bool                      m_evilTwinDeauth;

    // PCAP Logging
    class PCAPWriter* m_pcapWriter;

    // Internal tick handlers
    void tickScan();
    void tickDeauth();
    void tickBeaconFlood();
    void tickHandshakeCapture();
    void tickMonitor();

    // Frame builders (from Bruce)
    void buildDeauthFrame(uint8_t* frame, const uint8_t* dst,
                          const uint8_t* src, const uint8_t* bssid);
    void buildBeaconFrame(uint8_t* frame, size_t* len,
                          const char* ssid, const uint8_t* bssid,
                          uint8_t channel);

    // Promiscuous callback (static for C API)
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static BruceWiFi* s_instance;  // For static callback access
};

} // namespace Vanguard

#endif // ASSESSOR_BRUCE_WIFI_H
