/**
 * @file VanguardEngine.cpp
 * @brief The orchestrator - WiFi scanning with proper timing
 */

#include "VanguardEngine.h"
#include "SystemTask.h"
#include "../adapters/BruceWiFi.h"
#include "../adapters/BruceBLE.h"
#include "../adapters/EvilPortal.h"
#include "../adapters/BruceIR.h"
#include "SDManager.h"
#include "RadioWarden.h"
#include "../ui/FeedbackManager.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <cstring>

namespace Vanguard {



// =============================================================================
// SINGLETON
// =============================================================================

VanguardEngine& VanguardEngine::getInstance() {
    static VanguardEngine instance;
    return instance;
}

VanguardEngine::VanguardEngine()
    : m_initialized(false)
    , m_scanState(ScanState::IDLE)
    , m_scanProgress(0)
    , m_actionActive(false)
    , m_combinedScan(false)
    , m_onScanProgress(nullptr)
    , m_onActionProgress(nullptr)
    , m_scanStartMs(0)
    , m_transitionStep(0)
    , m_transitionStartMs(0)
    , m_transitionTotalStartMs(0)
    , m_bleInitAttempts(0)
    , m_reconChannelCount(0)
    , m_reconCurrentIdx(0)
    , m_reconWaiting(false)
{
    memset(m_reconChannels, 0, sizeof(m_reconChannels));
    m_actionProgress.type = ActionType::NONE;
    m_actionProgress.result = ActionResult::SUCCESS;
    m_actionProgress.packetsSent = 0;
    m_widsAlert.type = WidsEventType::NONE;
    m_widsAlert.active = false;
    m_widsAlert.count = 0;
    m_widsAlert.timestamp = 0;
}

VanguardEngine::~VanguardEngine() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool VanguardEngine::init() {
    if (m_initialized) return true;

    if (Serial) {
        Serial.println("[ENGINE] Initializing...");
    }

    // IR is lazy-initialized on first use (RMT conflicts at boot)
    // BLE and WiFi are also lazy-initialized
    
    m_initialized = true;

    // Step 5: Initialize SD Card
    SDManager::getInstance().init();

    // Step 7: Add virtual targets
    m_targetTable.addVirtualTarget("Universal Remote", TargetType::IR_DEVICE);

    // Wire up BruceWiFi associations
    BruceWiFi::getInstance().onAssociation([this](const uint8_t* client, const uint8_t* ap) {
        if (this->m_targetTable.addAssociation(client, ap)) {
             FeedbackManager::getInstance().pulse(50); // Feedback on client discovery
        }
    });

    // Wire up WIDS Alerts
    BruceWiFi::getInstance().onWidsAlert([this](WidsEventType type, int count) {
        m_widsAlert.type = type;
        m_widsAlert.count = count;
        m_widsAlert.timestamp = millis();
        m_widsAlert.active = true;
        
        // Audio Feedback
        if (type == WidsEventType::DEAUTH_FLOOD) {
             FeedbackManager::getInstance().beep(4000, 200); 
        } else {
             FeedbackManager::getInstance().beep(2000, 500);
        }
        
        if (Serial) {
             Serial.printf("[WIDS] Alert! Type %d Count %d\n", (int)type, count);
        }
    });

    if (Serial) {
        Serial.printf("[ENGINE] Ready\n");
    }
    return true;
}

void VanguardEngine::shutdown() {
    RadioWarden::getInstance().releaseRadio();
    m_initialized = false;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void VanguardEngine::tick() {
    // Poll for events from System Task (Core 0)
    // Cap at 10 events per tick to prevent Core 1 stall
    SystemEvent evt;
    int eventsProcessed = 0;
    while (SystemTask::getInstance().receiveEvent(evt) && eventsProcessed < 10) {
        handleSystemEvent(evt);
        eventsProcessed++;
    }

    // Handle WiFi->BLE transition state machine
    if (m_scanState == ScanState::TRANSITIONING_TO_BLE) {
        tickTransition();
    }

    // Handle post-scan recon phase
    if (m_scanState == ScanState::RECON) {
        tickRecon();
    }

    // Interpolate BLE scan progress (50% to 99%) + safety timeout
    if (m_scanState == ScanState::BLE_SCANNING) {
        uint32_t elapsed = millis() - m_scanStartMs;
        uint8_t progress = 50 + (uint8_t)((elapsed * 49) / 3000);
        if (progress > 99) progress = 99;
        if (progress != m_scanProgress) {
            m_scanProgress = progress;
            if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
        }

        // Safety timeout: if BLE_SCANNING for > 10 seconds, force recon
        if (elapsed > 10000) {
            if (Serial) Serial.println("[ENGINE] BLE scan safety timeout (10s)");
            BruceBLE::getInstance().stopScan();
            startRecon();
        }
    }
}

// =============================================================================
// SCANNING (IPC to Core 0)
// =============================================================================

void VanguardEngine::beginScan() {
    if (Serial) Serial.println("[SCAN] === BEGIN COMBINED SCAN ===");

    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = true;
    m_scanState = ScanState::WIFI_SCANNING;

    // Send Request: Start WiFi Scan
    SystemRequest req;
    req.cmd = SysCommand::WIFI_SCAN_START;
    req.payload = nullptr;
    SystemTask::getInstance().sendRequest(req);
    
    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
}

void VanguardEngine::beginWiFiScan() {
    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = false;
    m_scanState = ScanState::WIFI_SCANNING;

    SystemRequest req;
    req.cmd = SysCommand::WIFI_SCAN_START;
    SystemTask::getInstance().sendRequest(req);
    
    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
}

void VanguardEngine::beginBLEScan() {
    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = false;
    m_scanState = ScanState::BLE_SCANNING;

    SystemRequest req;
    req.cmd = SysCommand::BLE_SCAN_START;
    req.payload = (void*)(uintptr_t)10000; // 10 sec duration
    SystemTask::getInstance().sendRequest(req);
    
    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
}

void VanguardEngine::stopScan() {
    SystemRequest req;
    req.cmd = SysCommand::WIFI_SCAN_STOP;
    SystemTask::getInstance().sendRequest(req);

    req.cmd = SysCommand::BLE_SCAN_STOP;
    SystemTask::getInstance().sendRequest(req);

    // Stop recon if active
    if (m_scanState == ScanState::RECON) {
        req.cmd = SysCommand::RECON_STOP;
        SystemTask::getInstance().sendRequest(req);
    }

    m_scanState = ScanState::IDLE;
    m_combinedScan = false;
}

// =============================================================================
// EVENT HANDLING (From Core 0)
// =============================================================================

void VanguardEngine::handleSystemEvent(const SystemEvent& evt) {
    switch (evt.type) {
        case SysEventType::WIFI_SCAN_COMPLETE:
        {
            int count = (int)(intptr_t)evt.data;
            if (Serial) Serial.printf("[ENGINE] WiFi Scan Complete: %d\n", count);

            // Process results and handle BLE transition if combined scan
            processScanResults(count);
            break;
        }
        
        case SysEventType::BLE_DEVICE_FOUND:
        {
            if (!evt.data) break;
            BLEDeviceInfo* dev = (BLEDeviceInfo*)evt.data;
            // Add to table
            // We need to map BLEDeviceInfo to Target
            Target target;
            memset(&target, 0, sizeof(Target));
            target.type = TargetType::BLE_DEVICE;
            memcpy(target.bssid, dev->address, 6);
            strncpy(target.ssid, dev->name, SSID_MAX_LEN);
            target.ssid[SSID_MAX_LEN] = '\0';
            target.rssi = dev->rssi;
            target.firstSeenMs = dev->lastSeenMs;
            target.lastSeenMs = dev->lastSeenMs;

            m_targetTable.addOrUpdate(target);
            if (evt.isPointer) delete dev;
            break;
        }
        
        case SysEventType::BLE_SCAN_COMPLETE:
        {
            if (Serial) Serial.println("[ENGINE] BLE Scan Complete");
            // If this was a combined scan, start recon for client discovery
            if (m_combinedScan) {
                startRecon();
            } else {
                m_scanState = ScanState::COMPLETE;
                m_scanProgress = 100;
            }
            break;
        }

        case SysEventType::ASSOCIATION_FOUND:
        {
            if (!evt.data) break;
            AssociationEvent* assoc = (AssociationEvent*)evt.data;
            if (m_targetTable.addAssociation(assoc->station, assoc->bssid)) {
                FeedbackManager::getInstance().pulse(50);
            }
            if (evt.isPointer) delete assoc;
            break;
        }

        case SysEventType::RECON_DONE:
        {
            int found = (int)(intptr_t)evt.data;
            if (Serial) Serial.printf("[ENGINE] Recon channel done, %d associations\n", found);
            m_reconWaiting = false;  // tickRecon() will advance to next channel
            break;
        }

        case SysEventType::ACTION_PROGRESS:
        {
            if (!evt.data) break;
            ActionProgress* prog = (ActionProgress*)evt.data;
            m_actionProgress = *prog; // Copy progress
            if (m_onActionProgress) m_onActionProgress(m_actionProgress);
            if (evt.isPointer) delete prog;
            break;
        }

        case SysEventType::ACTION_COMPLETE:
        {
            m_actionActive = false;
            m_actionProgress.result = (ActionResult)(intptr_t)evt.data;
            if (m_onActionProgress) m_onActionProgress(m_actionProgress);
            break;
        }
        
        case SysEventType::ERROR_OCCURRED:
        {
            const char* msg = (const char*)evt.data;
            if (Serial) Serial.printf("[ENGINE] ERROR: %s\n", msg ? msg : "unknown");
            m_actionActive = false;
            m_actionProgress.result = ActionResult::FAILED_HARDWARE;
            strncpy(m_actionProgress.statusText, msg ? msg : "Error", sizeof(m_actionProgress.statusText) - 1);
            m_actionProgress.statusText[sizeof(m_actionProgress.statusText) - 1] = '\0';
            if (m_onActionProgress) m_onActionProgress(m_actionProgress);
            break;
        }

        default:
            break;
    }
    
    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
}

bool VanguardEngine::isCombinedScan() const {
    return m_combinedScan;
}

ScanState VanguardEngine::getScanState() const {
    return m_scanState;
}

uint8_t VanguardEngine::getScanProgress() const {
    return m_scanProgress;
}

uint32_t VanguardEngine::getScanElapsedMs() const {
    if (m_scanState == ScanState::IDLE || m_scanState == ScanState::COMPLETE || m_scanState == ScanState::ERROR) return 0;
    return millis() - m_scanStartMs;
}

void VanguardEngine::onScanProgress(ScanProgressCallback cb) {
    m_onScanProgress = cb;
}

void VanguardEngine::processScanResults(int count) {
    for (int i = 0; i < count; i++) {
        Target target;
        memset(&target, 0, sizeof(Target));

        // Get BSSID
        uint8_t* bssid = WiFi.BSSID(i);
        if (bssid) {
            memcpy(target.bssid, bssid, 6);
        }

        // Get SSID
        String ssid = WiFi.SSID(i);
        strncpy(target.ssid, ssid.c_str(), SSID_MAX_LEN);
        target.ssid[SSID_MAX_LEN] = '\0';

        target.type = TargetType::ACCESS_POINT;
        target.channel = WiFi.channel(i);
        target.rssi = WiFi.RSSI(i);

        // Map encryption
        wifi_auth_mode_t enc = WiFi.encryptionType(i);
        switch (enc) {
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
            strncpy(target.ssid, "[Hidden]", sizeof(target.ssid) - 1);
            target.ssid[sizeof(target.ssid) - 1] = '\0';
        }

        target.firstSeenMs = millis();
        target.lastSeenMs = millis();
        target.beaconCount = 1;
        target.clientCount = 0;

        m_targetTable.addOrUpdate(target);
    }

    WiFi.scanDelete();  // Free memory

    // If combined scan, start NON-BLOCKING transition to BLE
    if (m_combinedScan) {
        if (Serial) {
            Serial.println("[SCAN] WiFi done, starting BLE transition...");
        }

        // Start the transition state machine
        m_scanState = ScanState::TRANSITIONING_TO_BLE;
        m_transitionStep = 0;
        m_transitionStartMs = millis();
        m_transitionTotalStartMs = millis();
        m_bleInitAttempts = 0;
        m_scanProgress = 46;  // Just past WiFi

        if (m_onScanProgress) {
            m_onScanProgress(m_scanState, m_scanProgress);
        }
    } else {
        // WiFi-only scan done, start recon for client discovery
        startRecon();
    }
}

// =============================================================================
// NON-BLOCKING WIFI→BLE TRANSITION
// =============================================================================

void VanguardEngine::tickTransition() {
    // State machine for WiFi→BLE transition
    // Each step does minimal work and returns, next tick continues
    uint32_t elapsed = millis() - m_transitionStartMs;

    switch (m_transitionStep) {
        case 0:
            // Step 0: Stop WiFi activity
            BruceWiFi::getInstance().onDisable();
            m_transitionStep = 2; // Warden makes transition faster, jump to BLE
            m_transitionStartMs = millis();
            m_scanProgress = 46;
            if (Serial) Serial.println("[SCAN] Step 0: WiFi disable");
            break;


        case 2:
            // Step 2: Wait 50ms for radio to fully stop (BLE shutdown is instant when not running)
            if (elapsed >= 50) {
                BruceBLE& ble = BruceBLE::getInstance();
                ble.shutdown();
                m_transitionStep = 3;
                m_transitionStartMs = millis();
                m_scanProgress = 48;
                if (Serial) Serial.println("[SCAN] Step 2: BLE shutdown");
            }
            break;

        case 3:
            // Step 3: Wait 50ms, then try BLE init
            if (elapsed >= 50) {
                m_transitionStep = 4;
                m_transitionStartMs = millis();
                m_bleInitAttempts = 0;
                if (Serial) Serial.println("[SCAN] Step 3: Ready for BLE init");
            }
            break;

        case 4:
            // Step 4: Try BLE init (one attempt per tick)
            {
                BruceBLE& ble = BruceBLE::getInstance();
                m_bleInitAttempts++;

                // Show progress movement so user knows it's not stuck
                m_scanProgress = 48 + m_bleInitAttempts;
                if (m_onScanProgress) {
                    m_onScanProgress(m_scanState, m_scanProgress);
                }

                if (Serial) {
                    Serial.printf("[SCAN] Step 4: BLE init attempt %d\n", m_bleInitAttempts);
                }

                uint32_t initStart = millis();
                bool initOk = ble.init();
                uint32_t initElapsed = millis() - initStart;

                if (Serial) {
                    Serial.printf("[SCAN] BLE init took %ums\n", initElapsed);
                }

                if (initOk) {
                    // Success! Start BLE scan
                    m_transitionStep = 5;
                    m_transitionStartMs = millis();
                    m_scanProgress = 49;
                    if (Serial) Serial.println("[SCAN] BLE init SUCCESS");
                } else if (m_bleInitAttempts >= 3) {
                    // Failed after 3 attempts, still do recon with WiFi results
                    if (Serial) Serial.println("[SCAN] BLE init FAILED after 3 attempts, starting recon");
                    startRecon();
                } else {
                    // Wait before retry
                    m_transitionStep = 100;  // Wait state
                    m_transitionStartMs = millis();
                }
            }
            break;

        case 5:
            // Step 5: Start BLE scan
            {
                BruceBLE& ble = BruceBLE::getInstance();

                // Register callbacks BEFORE starting scan (bypasses SystemTask IPC)
                ble.onDeviceFound([this](const BLEDeviceInfo& dev) {
                    Target target;
                    memset(&target, 0, sizeof(Target));
                    target.type = TargetType::BLE_DEVICE;
                    memcpy(target.bssid, dev.address, 6);
                    strncpy(target.ssid, dev.name, SSID_MAX_LEN);
                    target.ssid[SSID_MAX_LEN] = '\0';
                    target.rssi = dev.rssi;
                    target.firstSeenMs = dev.lastSeenMs;
                    target.lastSeenMs = dev.lastSeenMs;
                    m_targetTable.addOrUpdate(target);
                });

                ble.onScanComplete([this](int count) {
                    if (Serial) Serial.printf("[ENGINE] BLE scan complete: %d devices\n", count);
                    startRecon();
                });

                bool scanOk = ble.beginScan(3000);

                if (!scanOk) {
                    // BLE scan failed, still do recon with WiFi-only results
                    if (Serial) Serial.println("[SCAN] BLE scan start FAILED, starting recon");
                    startRecon();
                    break;
                }

                m_scanState = ScanState::BLE_SCANNING;
                m_scanProgress = 50;
                m_scanStartMs = millis();

                if (Serial) Serial.println("[SCAN] Step 5: BLE scan started");

                if (m_onScanProgress) {
                    m_onScanProgress(m_scanState, m_scanProgress);
                }
            }
            break;

        case 100:
            // Wait state: wait 200ms between retries for stable radio state
            if (elapsed >= 200) {
                m_transitionStep = 4;
                m_transitionStartMs = millis();
            }
            break;

        default:
            // Shouldn't happen, force complete
            m_scanState = ScanState::COMPLETE;
            m_scanProgress = 100;
            break;
    }

    // Safety timeout for entire transition: 5 seconds total
    if (m_scanState == ScanState::TRANSITIONING_TO_BLE) {
        uint32_t totalElapsed = millis() - m_transitionTotalStartMs;
        if (totalElapsed > 5000) {
            if (Serial) Serial.println("[SCAN] Total timeout (5s), starting recon");
            startRecon();
        }
    }
}

// =============================================================================
// POST-SCAN RECON (Client Discovery)
// =============================================================================

void VanguardEngine::startRecon() {
    // Collect unique channels from discovered WiFi APs, sorted by strongest RSSI
    const auto& targets = m_targetTable.getAll();

    // Build channel list from APs, prioritizing strongest signals
    struct ChannelInfo {
        uint8_t channel;
        int8_t bestRssi;
    };
    ChannelInfo channels[14];
    uint8_t channelCount = 0;

    for (const auto& t : targets) {
        if (t.type != TargetType::ACCESS_POINT) continue;
        if (t.channel < 1 || t.channel > 14) continue;

        // Check if channel already tracked
        bool found = false;
        for (uint8_t i = 0; i < channelCount; i++) {
            if (channels[i].channel == t.channel) {
                if (t.rssi > channels[i].bestRssi) channels[i].bestRssi = t.rssi;
                found = true;
                break;
            }
        }
        if (!found && channelCount < 14) {
            channels[channelCount].channel = t.channel;
            channels[channelCount].bestRssi = t.rssi;
            channelCount++;
        }
    }

    if (channelCount == 0) {
        // No APs found, skip recon
        if (Serial) Serial.println("[ENGINE] No APs for recon, completing scan");
        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;
        if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
        return;
    }

    // Sort channels by RSSI (strongest first) so we discover clients on best APs first
    for (uint8_t i = 0; i < channelCount - 1; i++) {
        for (uint8_t j = i + 1; j < channelCount; j++) {
            if (channels[j].bestRssi > channels[i].bestRssi) {
                ChannelInfo tmp = channels[i];
                channels[i] = channels[j];
                channels[j] = tmp;
            }
        }
    }

    // Cap at 8 channels to keep recon under 16 seconds
    if (channelCount > 8) channelCount = 8;

    m_reconChannelCount = channelCount;
    m_reconCurrentIdx = 0;
    m_reconWaiting = false;
    for (uint8_t i = 0; i < channelCount; i++) {
        m_reconChannels[i] = channels[i].channel;
    }

    m_scanState = ScanState::RECON;
    m_scanProgress = 90;  // Recon is 90-99%

    if (Serial) {
        Serial.printf("[ENGINE] Starting recon on %d channels:", channelCount);
        for (uint8_t i = 0; i < channelCount; i++) Serial.printf(" %d", m_reconChannels[i]);
        Serial.println();
    }

    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);

    // Send first channel request
    SystemRequest req;
    req.cmd = SysCommand::RECON_CHANNEL;
    req.payload = (void*)(uintptr_t)m_reconChannels[0];
    SystemTask::getInstance().sendRequest(req);
    m_reconWaiting = true;
}

void VanguardEngine::tickRecon() {
    if (m_reconWaiting) return;  // Still waiting for RECON_DONE from SystemTask

    // Advance to next channel
    m_reconCurrentIdx++;

    if (m_reconCurrentIdx >= m_reconChannelCount) {
        // All channels scanned, complete
        if (Serial) Serial.println("[ENGINE] Recon complete");

        // Stop recon on SystemTask side
        SystemRequest req;
        req.cmd = SysCommand::RECON_STOP;
        SystemTask::getInstance().sendRequest(req);

        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;
        if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);
        return;
    }

    // Update progress (90-99 range across channels)
    m_scanProgress = 90 + (uint8_t)((m_reconCurrentIdx * 9) / m_reconChannelCount);
    if (m_onScanProgress) m_onScanProgress(m_scanState, m_scanProgress);

    // Send next channel
    if (Serial) Serial.printf("[ENGINE] Recon channel %d/%d (ch %d)\n",
        m_reconCurrentIdx + 1, m_reconChannelCount, m_reconChannels[m_reconCurrentIdx]);

    SystemRequest req;
    req.cmd = SysCommand::RECON_CHANNEL;
    req.payload = (void*)(uintptr_t)m_reconChannels[m_reconCurrentIdx];
    SystemTask::getInstance().sendRequest(req);
    m_reconWaiting = true;
}

// =============================================================================
// TARGETS
// =============================================================================

const std::vector<Target>& VanguardEngine::getTargets() const {
    return m_targetTable.getAll();
}

size_t VanguardEngine::getTargetCount() const {
    return m_targetTable.count();
}

std::vector<Target> VanguardEngine::getFilteredTargets(const TargetFilter& filter,
                                                        SortOrder order) const {
    return m_targetTable.getFiltered(filter, order);
}

const Target* VanguardEngine::findTarget(const uint8_t* bssid) const {
    return m_targetTable.findByBssid(bssid);
}

void VanguardEngine::clearTargets() {
    m_targetTable.clear();
}

// =============================================================================
// ACTIONS
// =============================================================================

std::vector<AvailableAction> VanguardEngine::getActionsFor(const Target& target) const {
    return m_actionResolver.getActionsFor(target);
}

bool VanguardEngine::executeAction(ActionType action, const Target& target, const uint8_t* stationMac) {
    // Reset progress
    m_actionProgress.type = action;
    m_actionProgress.result = ActionResult::IN_PROGRESS;
    m_actionProgress.packetsSent = 0;
    m_actionProgress.elapsedMs = 0;
    strncpy(m_actionProgress.statusText, "Starting...", sizeof(m_actionProgress.statusText) - 1);
    m_actionProgress.statusText[sizeof(m_actionProgress.statusText) - 1] = '\0';
    m_actionActive = true;

    // Check for 5GHz limitation
    if (target.channel > 14) {
        m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
        strncpy(m_actionProgress.statusText, "5GHz not supported", sizeof(m_actionProgress.statusText) - 1);
        m_actionProgress.statusText[sizeof(m_actionProgress.statusText) - 1] = '\0';
        m_actionActive = false;
        return false;
    }

    // Allocate Request Payload
    ActionRequest* req = new ActionRequest();
    req->type = action;
    req->target = target; // Copy target
    
    if (stationMac) {
        memcpy(req->stationMac, stationMac, 6);
    } else {
        memset(req->stationMac, 0, 6);
    }
    
    // Send Request
    SystemRequest sysReq;
    sysReq.cmd = SysCommand::ACTION_START;
    sysReq.payload = req;
    sysReq.freeCb = [](void* p) { delete (ActionRequest*)p; };
    
    SystemTask::getInstance().sendRequest(sysReq);
    
    return true;
}

void VanguardEngine::stopAction() {
    SystemRequest req;
    req.cmd = SysCommand::ACTION_STOP;
    SystemTask::getInstance().sendRequest(req);

    m_actionActive = false;
    m_actionProgress.result = ActionResult::STOPPED;
    strncpy(m_actionProgress.statusText, "Stopped", sizeof(m_actionProgress.statusText) - 1);
    m_actionProgress.statusText[sizeof(m_actionProgress.statusText) - 1] = '\0';

    if (Serial) {
        Serial.println("[ENGINE] Stopping action...");
    }
}

bool VanguardEngine::isActionActive() const {
    return m_actionActive;
}

ActionProgress VanguardEngine::getActionProgress() const {
    return m_actionProgress;
}

void VanguardEngine::onActionProgress(ActionProgressCallback cb) {
    m_onActionProgress = cb;
}

// =============================================================================
// HARDWARE STATUS
// =============================================================================

bool VanguardEngine::hasWiFi() const {
    return m_initialized;
}

bool VanguardEngine::hasBLE() const {
    return true;
}

bool VanguardEngine::hasRF() const {
    return false;
}

bool VanguardEngine::hasIR() const {
    return true;
}

const WidsAlertState& VanguardEngine::getWidsAlert() const {
    return m_widsAlert;
}

void VanguardEngine::clearWidsAlert() {
    m_widsAlert.active = false;
}

} // namespace Vanguard
