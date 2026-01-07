/**
 * @file AssessorEngine.cpp
 * @brief The orchestrator - ties scanning, targets, and actions together
 */

#include "AssessorEngine.h"
#include "../adapters/BruceWiFi.h"

namespace Assessor {

// =============================================================================
// SINGLETON
// =============================================================================

AssessorEngine& AssessorEngine::getInstance() {
    static AssessorEngine instance;
    return instance;
}

AssessorEngine::AssessorEngine()
    : m_initialized(false)
    , m_scanState(ScanState::IDLE)
    , m_scanProgress(0)
    , m_actionActive(false)
    , m_onScanProgress(nullptr)
    , m_onActionProgress(nullptr)
{
    m_actionProgress.type = ActionType::NONE;
    m_actionProgress.result = ActionResult::SUCCESS;
    m_actionProgress.packetsSent = 0;
}

AssessorEngine::~AssessorEngine() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool AssessorEngine::init() {
    if (m_initialized) return true;

    // Initialize WiFi adapter
    if (!BruceWiFi::getInstance().init()) {
        return false;
    }

    // Register scan callback
    BruceWiFi::getInstance().onScanComplete([this](int count) {
        processScanResults(count);
    });

    // Register attack progress callback
    BruceWiFi::getInstance().onAttackProgress([this](uint32_t packets) {
        m_actionProgress.packetsSent = packets;
        m_actionProgress.elapsedMs = millis() - m_actionProgress.startTimeMs;
        if (m_onActionProgress) {
            m_onActionProgress(m_actionProgress);
        }
    });

    m_initialized = true;
    return true;
}

void AssessorEngine::shutdown() {
    stopScan();
    stopAction();
    BruceWiFi::getInstance().shutdown();
    m_initialized = false;
}

void AssessorEngine::tick() {
    if (!m_initialized) return;

    BruceWiFi::getInstance().tick();
    tickScan();
    tickAction();

    // Periodic stale target pruning
    static uint32_t lastPruneMs = 0;
    uint32_t now = millis();
    if (now - lastPruneMs > 10000) {  // Every 10 seconds
        m_targetTable.pruneStale(now);
        lastPruneMs = now;
    }
}

// =============================================================================
// SCANNING
// =============================================================================

void AssessorEngine::beginScan() {
    beginWiFiScan();
    // TODO: Add BLE scan after WiFi completes
}

void AssessorEngine::beginWiFiScan() {
    m_scanState = ScanState::WIFI_SCANNING;
    m_scanProgress = 0;
    BruceWiFi::getInstance().beginScan();

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void AssessorEngine::beginBLEScan() {
    m_scanState = ScanState::BLE_SCANNING;
    // TODO: Implement BLE scanning
}

void AssessorEngine::stopScan() {
    BruceWiFi::getInstance().stopScan();
    m_scanState = ScanState::IDLE;
}

ScanState AssessorEngine::getScanState() const {
    return m_scanState;
}

uint8_t AssessorEngine::getScanProgress() const {
    return m_scanProgress;
}

void AssessorEngine::onScanProgress(ScanProgressCallback cb) {
    m_onScanProgress = cb;
}

void AssessorEngine::tickScan() {
    if (m_scanState == ScanState::WIFI_SCANNING) {
        if (BruceWiFi::getInstance().isScanComplete()) {
            // Results will be processed via callback
        }
    }
}

void AssessorEngine::processScanResults(int count) {
    BruceWiFi& wifi = BruceWiFi::getInstance();

    for (int i = 0; i < count; i++) {
        ScanResultEntry entry;
        if (!wifi.getScanResult(i, entry)) continue;

        Target target;
        memset(&target, 0, sizeof(Target));

        memcpy(target.bssid, entry.bssid, 6);
        strncpy(target.ssid, entry.ssid, SSID_MAX_LEN);
        target.ssid[SSID_MAX_LEN] = '\0';

        target.type = TargetType::ACCESS_POINT;
        target.channel = entry.channel;
        target.rssi = entry.rssi;

        // Map encryption type to our SecurityType
        switch (entry.encryptionType) {
            case WIFI_AUTH_OPEN:
                target.security = SecurityType::OPEN;
                break;
            case WIFI_AUTH_WEP:
                target.security = SecurityType::WEP;
                break;
            case WIFI_AUTH_WPA_PSK:
                target.security = SecurityType::WPA_PSK;
                break;
            case WIFI_AUTH_WPA2_PSK:
                target.security = SecurityType::WPA2_PSK;
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                target.security = SecurityType::WPA2_PSK;
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                target.security = SecurityType::WPA2_ENTERPRISE;
                break;
            case WIFI_AUTH_WPA3_PSK:
                target.security = SecurityType::WPA3_SAE;
                break;
            default:
                target.security = SecurityType::UNKNOWN;
        }

        target.isHidden = (strlen(target.ssid) == 0);
        if (target.isHidden) {
            strcpy(target.ssid, "[Hidden]");
        }

        target.firstSeenMs = millis();
        target.lastSeenMs = millis();
        target.beaconCount = 1;
        target.clientCount = 0;  // Will be updated via monitoring

        m_targetTable.addOrUpdate(target);
    }

    m_scanState = ScanState::COMPLETE;
    m_scanProgress = 100;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

// =============================================================================
// TARGETS
// =============================================================================

const std::vector<Target>& AssessorEngine::getTargets() const {
    return m_targetTable.getAll();
}

size_t AssessorEngine::getTargetCount() const {
    return m_targetTable.count();
}

std::vector<Target> AssessorEngine::getFilteredTargets(const TargetFilter& filter,
                                                        SortOrder order) const {
    return m_targetTable.getFiltered(filter, order);
}

const Target* AssessorEngine::findTarget(const uint8_t* bssid) const {
    return m_targetTable.findByBssid(bssid);
}

void AssessorEngine::clearTargets() {
    m_targetTable.clear();
}

// =============================================================================
// ACTIONS
// =============================================================================

std::vector<AvailableAction> AssessorEngine::getActionsFor(const Target& target) const {
    return m_actionResolver.getActionsFor(target);
}

bool AssessorEngine::executeAction(ActionType action, const Target& target) {
    if (m_actionActive) {
        stopAction();
    }

    // Verify action is valid
    if (!m_actionResolver.isActionValid(target, action)) {
        return false;
    }

    BruceWiFi& wifi = BruceWiFi::getInstance();
    bool started = false;

    m_actionProgress.type = action;
    m_actionProgress.result = ActionResult::IN_PROGRESS;
    m_actionProgress.startTimeMs = millis();
    m_actionProgress.elapsedMs = 0;
    m_actionProgress.packetsSent = 0;

    switch (action) {
        case ActionType::DEAUTH_ALL:
            started = wifi.deauthAll(target.bssid, target.channel);
            m_actionProgress.statusText = "Sending deauth frames...";
            break;

        case ActionType::DEAUTH_SINGLE:
            // For single deauth, we'd need client selection
            // For now, fall back to broadcast
            started = wifi.deauthAll(target.bssid, target.channel);
            m_actionProgress.statusText = "Deauthing client...";
            break;

        case ActionType::BEACON_FLOOD: {
            const char* ssids[] = { target.ssid };
            started = wifi.beaconFlood(ssids, 1, target.channel);
            m_actionProgress.statusText = "Flooding beacons...";
            break;
        }

        case ActionType::CAPTURE_HANDSHAKE:
            started = wifi.captureHandshake(target.bssid, target.channel, true);
            m_actionProgress.statusText = "Capturing handshake...";
            break;

        case ActionType::MONITOR:
            started = wifi.startMonitor(target.channel);
            m_actionProgress.statusText = "Monitoring...";
            break;

        case ActionType::EVIL_TWIN:
            started = wifi.startEvilTwin(target.ssid, target.channel, true);
            m_actionProgress.statusText = "Evil twin active...";
            break;

        default:
            m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
            return false;
    }

    if (started) {
        m_actionActive = true;
        if (m_onActionProgress) {
            m_onActionProgress(m_actionProgress);
        }
    } else {
        m_actionProgress.result = ActionResult::FAILED_HARDWARE;
    }

    return started;
}

void AssessorEngine::stopAction() {
    BruceWiFi::getInstance().stopAttack();
    m_actionActive = false;
    m_actionProgress.result = ActionResult::CANCELLED;
}

bool AssessorEngine::isActionActive() const {
    return m_actionActive;
}

ActionProgress AssessorEngine::getActionProgress() const {
    return m_actionProgress;
}

void AssessorEngine::onActionProgress(ActionProgressCallback cb) {
    m_onActionProgress = cb;
}

void AssessorEngine::tickAction() {
    if (!m_actionActive) return;

    WiFiAdapterState state = BruceWiFi::getInstance().getState();

    if (state == WiFiAdapterState::IDLE || state == WiFiAdapterState::ERROR) {
        // Attack finished
        m_actionActive = false;
        m_actionProgress.result = (state == WiFiAdapterState::ERROR)
            ? ActionResult::FAILED_HARDWARE
            : ActionResult::SUCCESS;

        if (m_onActionProgress) {
            m_onActionProgress(m_actionProgress);
        }
    }
}

// =============================================================================
// HARDWARE STATUS
// =============================================================================

bool AssessorEngine::hasWiFi() const {
    return m_initialized;
}

bool AssessorEngine::hasBLE() const {
    return true;  // Cardputer has BLE
}

bool AssessorEngine::hasRF() const {
    return false;  // Base Cardputer doesn't have sub-GHz
}

bool AssessorEngine::hasIR() const {
    return true;  // Cardputer has IR
}

} // namespace Assessor
