#include "SystemTask.h"
#include "RadioWarden.h"
#include <esp_task_wdt.h>
#include "../adapters/BruceWiFi.h"
#include "../adapters/BruceBLE.h"
#include "../adapters/BruceIR.h"
#include "../adapters/EvilPortal.h"

namespace Vanguard {

SystemTask& SystemTask::getInstance() {
    static SystemTask instance;
    return instance;
}

SystemTask::SystemTask() :
    m_taskHandle(nullptr),
    m_running(false),
    m_actionActive(false),
    m_currentAction(ActionType::NONE),
    m_actionStartTime(0),
    m_lastProgressTime(0),
    m_droppedEvents(0)
{
    // Create Queues
    // Request Queue: UI -> System (Capacity 10)
    m_reqQueue = xQueueCreate(10, sizeof(SystemRequest));

    // Event Queue: System -> UI (Capacity 40 - BLE scans generate many device events)
    m_evtQueue = xQueueCreate(40, sizeof(SystemEvent));

    if (!m_reqQueue || !m_evtQueue) {
        if (Serial) Serial.println("[SYSTEM] FATAL: Queue creation failed");
        return;
    }
}

void SystemTask::start() {
    if (m_running) return;

    // Start task on Core 0
    // Stack size: 16384 bytes (WiFi promiscuous + BLE NimBLE + IR need room)
    xTaskCreatePinnedToCore(
        taskLoop,
        "SystemTask",
        16384,
        this,
        1,              // Priority 1 (Low-ish, let default tasks run)
        &m_taskHandle,
        0               // Core 0
    );

    m_running = true;
    if (Serial) Serial.println("[SYSTEM] Task started on Core 0");
}

// WDT Architecture:
// - Core 0 (this task): subscribed via esp_task_wdt_add(), feeds every loop iteration
// - Core 1 (Arduino loop): feeds via esp_task_wdt_reset() in main loop()
// - Default timeout: 5 seconds. If either core stalls, WDT triggers system reset.
// - Stack: 16384 bytes. Monitor via uxTaskGetStackHighWaterMark() (see I4 check below).
void SystemTask::subscribeWatchdog() {
    // Subscribe this task to the Task WDT so esp_task_wdt_reset() works.
    // Without this, the Core 0 idle task (which IS subscribed) starves
    // because our tight loop never yields long enough for it to run.
    esp_task_wdt_add(NULL);  // NULL = current task
    if (Serial) Serial.println("[SYSTEM] Subscribed to Task WDT");
}

void SystemTask::taskLoop(void* param) {
    SystemTask* self = (SystemTask*)param;
    self->run();
}

void SystemTask::run() {
    subscribeWatchdog();

    SystemRequest req;

    while (true) {
        uint32_t now = millis();

        // 1. Process Requests (Non-blocking check?)
        // Actually, we can block here for a short time to save CPU if idle
        // But we also need to tick the adapters!
        
        // Wait 10ms for a request
        if (xQueueReceive(m_reqQueue, &req, pdMS_TO_TICKS(10)) == pdTRUE) {
            handleRequest(req);
            
            // Clean up payload if needed
            if (req.freeCb && req.payload) {
                req.freeCb(req.payload);
            }
        }
        
        // 2. Tick Active Adapters
        // This replaces the old VanguardEngine tick loop
        
        // WiFi Tick (Scanning/Attacking)
        BruceWiFi::getInstance().onTick();
        
        // BLE Tick
        BruceBLE::getInstance().onTick();
        
        // IR Tick
        BruceIR::getInstance().tick();
        
        // 3. Monitor Status changes and emit events
        if (m_actionActive) {

             bool completed = false;
             ActionResult result = ActionResult::SUCCESS;
             const char* statusMsg = nullptr;

             // Specific completion checks
             if (m_currentAction == ActionType::CAPTURE_HANDSHAKE) {
                 if (BruceWiFi::getInstance().hasHandshake()) {
                     completed = true;
                     result = ActionResult::SUCCESS;
                     statusMsg = "Handshake Captured!";
                 }
             }

             // Generic timeout for all timed actions
             uint32_t timeout = getActionTimeout(m_currentAction);
             if (!completed && timeout > 0 && (now - m_actionStartTime > timeout)) {
                 completed = true;
                 result = ActionResult::SUCCESS;
                 uint32_t packets = BruceWiFi::getInstance().getPacketsSent() + BruceBLE::getInstance().getAdvertisementsSent();
                 statusMsg = getCompletionMessage(m_currentAction, packets, now - m_actionStartTime);
             }

             if (completed) {
                 // Stop hardware
                 BruceWiFi::getInstance().stopHardwareActivities();
                 BruceBLE::getInstance().stopHardwareActivities();

                 m_actionActive = false;
                 sendEvent(SysEventType::ACTION_COMPLETE, (void*)(intptr_t)result, 0, false);
                 ActionProgress* finalProg = new ActionProgress();
                 if (!finalProg) { Serial.println("[SYSTEM] ERR: Heap allocation failed"); break; }
                 finalProg->type = m_currentAction;
                 finalProg->startTimeMs = m_actionStartTime;
                 finalProg->elapsedMs = now - m_actionStartTime;
                 finalProg->result = result;
                 finalProg->packetsSent = BruceWiFi::getInstance().getPacketsSent() + BruceBLE::getInstance().getAdvertisementsSent();
                 strncpy(finalProg->statusText, statusMsg ? statusMsg : "", sizeof(finalProg->statusText) - 1);
                 finalProg->statusText[sizeof(finalProg->statusText) - 1] = '\0';
                 sendEvent(SysEventType::ACTION_PROGRESS, finalProg, sizeof(ActionProgress), true);
             } else if (now - m_lastProgressTime > 500) {
                  ActionProgress* prog = new ActionProgress();
                  if (!prog) { Serial.println("[SYSTEM] ERR: Heap allocation failed"); break; }
                  prog->type = m_currentAction;
                  prog->elapsedMs = now - m_actionStartTime;
                  prog->result = ActionResult::IN_PROGRESS;

                  uint32_t wifiPackets = BruceWiFi::getInstance().getPacketsSent();
                  uint32_t blePackets = BruceBLE::getInstance().getAdvertisementsSent();
                  prog->packetsSent = wifiPackets + blePackets;

                  // Per-action status text
                  switch (m_currentAction) {
                      case ActionType::CAPTURE_HANDSHAKE:
                          strncpy(prog->statusText, "Sniffing EAPOL...", sizeof(prog->statusText) - 1);
                          break;
                      case ActionType::EVIL_TWIN: {
                          EvilPortal& portal = EvilPortal::getInstance();
                          int creds = portal.getCredentialCount();
                          if (creds > 0) {
                              snprintf(prog->statusText, sizeof(prog->statusText), "Portal: %dc %dcli",
                                       creds, portal.getClientCount());
                          } else {
                              snprintf(prog->statusText, sizeof(prog->statusText), "Portal: %d clients",
                                       portal.getClientCount());
                          }
                          break;
                      }
                      case ActionType::DEAUTH_ALL:
                      case ActionType::DEAUTH_SINGLE:
                          snprintf(prog->statusText, sizeof(prog->statusText), "Deauthing... %u sent", prog->packetsSent);
                          break;
                      case ActionType::BEACON_FLOOD:
                          snprintf(prog->statusText, sizeof(prog->statusText), "Beacons: %u sent", prog->packetsSent);
                          break;
                      case ActionType::BLE_SPAM:
                      case ActionType::BLE_SOUR_APPLE:
                          snprintf(prog->statusText, sizeof(prog->statusText), "BLE spam: %u ads", prog->packetsSent);
                          break;
                      case ActionType::PROBE_FLOOD:
                          snprintf(prog->statusText, sizeof(prog->statusText), "Probing... %u sent", prog->packetsSent);
                          break;
                      case ActionType::MONITOR:
                          snprintf(prog->statusText, sizeof(prog->statusText), "Monitoring... %u pkts", prog->packetsSent);
                          break;
                      case ActionType::CAPTURE_PMKID:
                          strncpy(prog->statusText, "Waiting for PMKID...", sizeof(prog->statusText) - 1);
                          break;
                      case ActionType::BLE_SKIMMER_DETECT:
                          strncpy(prog->statusText, "Scanning for skimmers...", sizeof(prog->statusText) - 1);
                          break;
                      default:
                          strncpy(prog->statusText, "Running...", sizeof(prog->statusText) - 1);
                          break;
                  }
                  prog->statusText[sizeof(prog->statusText) - 1] = '\0';

                  prog->startTimeMs = m_actionStartTime;
                  sendEvent(SysEventType::ACTION_PROGRESS, prog, sizeof(ActionProgress), true);
                  m_lastProgressTime = now;
             }
        }
        
        // Feed Watchdog for Core 0
        esp_task_wdt_reset();

        // Periodic stack check (every 30s)
        static uint32_t lastStackCheck = 0;
        if (now - lastStackCheck > 30000) {
            lastStackCheck = now;
            UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
            if (stackRemaining < 1024) {
                if (Serial) Serial.printf("[SYSTEM] WARNING: Stack low! %u bytes remaining\n", stackRemaining);
            }
        }
    }
}

bool SystemTask::sendRequest(const SystemRequest& req) {
    if (!m_running) return false;
    return xQueueSend(m_reqQueue, &req, 0) == pdTRUE;
}

bool SystemTask::receiveEvent(SystemEvent& evt) {
    if (!m_running) return false;
    return xQueueReceive(m_evtQueue, &evt, 0) == pdTRUE;
}

void SystemTask::sendEvent(SysEventType type, void* data, size_t len, bool isPtr) {
    SystemEvent evt;
    evt.type = type;
    evt.data = data;
    evt.dataLen = len;
    evt.isPointer = isPtr;

    if (xQueueSend(m_evtQueue, &evt, 0) != pdTRUE) {
        // Queue full. Clean up heap-allocated data to prevent leak.
        // Type-safe delete based on event type.
        if (isPtr && data) {
            switch (type) {
                case SysEventType::ACTION_PROGRESS:
                    delete static_cast<ActionProgress*>(data);
                    break;
                case SysEventType::BLE_DEVICE_FOUND:
                    delete static_cast<BLEDeviceInfo*>(data);
                    break;
                case SysEventType::ASSOCIATION_FOUND:
                    delete static_cast<AssociationEvent*>(data);
                    break;
                default:
                    delete static_cast<uint8_t*>(data);
                    break;
            }
        }
        m_droppedEvents++;
        if (Serial) Serial.printf("[SYSTEM] WARN: Event queue full (dropped %u total)\n", m_droppedEvents);
    }
}

// =============================================================================
// ACTION HELPERS
// =============================================================================

uint32_t SystemTask::getActionTimeout(ActionType type) const {
    switch (type) {
        case ActionType::DEAUTH_ALL:
        case ActionType::DEAUTH_SINGLE:    return 30000;
        case ActionType::BEACON_FLOOD:     return 60000;
        case ActionType::BLE_SPAM:
        case ActionType::BLE_SOUR_APPLE:   return 30000;
        case ActionType::PROBE_FLOOD:      return 30000;
        case ActionType::EVIL_TWIN:        return 300000;
        case ActionType::CAPTURE_HANDSHAKE:return 60000;
        case ActionType::CAPTURE_PMKID:    return 90000;
        case ActionType::MONITOR:          return 120000;
        case ActionType::BLE_SKIMMER_DETECT: return 6000;
        case ActionType::IR_REPLAY:
        case ActionType::IR_TVBGONE:       return 0; // instant
        default:                           return 30000;
    }
}

const char* SystemTask::getCompletionMessage(ActionType type, uint32_t packets, uint32_t elapsed) const {
    static char buf[64];
    uint32_t secs = elapsed / 1000;
    switch (type) {
        case ActionType::DEAUTH_ALL:
        case ActionType::DEAUTH_SINGLE:
            snprintf(buf, sizeof(buf), "%u deauth frames in %us", packets, secs);
            break;
        case ActionType::BEACON_FLOOD:
            snprintf(buf, sizeof(buf), "%u beacons broadcast", packets);
            break;
        case ActionType::BLE_SPAM:
        case ActionType::BLE_SOUR_APPLE:
            snprintf(buf, sizeof(buf), "%u advertisements sent", packets);
            break;
        case ActionType::PROBE_FLOOD:
            snprintf(buf, sizeof(buf), "%u probes sent in %us", packets, secs);
            break;
        case ActionType::MONITOR:
            snprintf(buf, sizeof(buf), "Captured %u packets", packets);
            break;
        case ActionType::EVIL_TWIN: {
            int creds = EvilPortal::getInstance().getCredentialCount();
            if (creds > 0) {
                snprintf(buf, sizeof(buf), "Captured %d credentials", creds);
            } else {
                snprintf(buf, sizeof(buf), "Portal served %u clients", packets);
            }
            break;
        }
        case ActionType::CAPTURE_PMKID:
            snprintf(buf, sizeof(buf), "PMKID scan complete (%us)", secs);
            break;
        case ActionType::BLE_SKIMMER_DETECT:
            snprintf(buf, sizeof(buf), "Scan complete: %u suspicious", packets);
            break;
        default:
            snprintf(buf, sizeof(buf), "Complete: %u packets in %us", packets, secs);
            break;
    }
    return buf;
}

// =============================================================================
// HANDLERS
// =============================================================================

void SystemTask::handleRequest(const SystemRequest& req) {
    switch (req.cmd) {
        case SysCommand::WIFI_SCAN_START:
            handleWiFiScanStart();
            break;
        case SysCommand::WIFI_SCAN_STOP:
            handleWiFiScanStop();
            break;
        case SysCommand::BLE_SCAN_START:
            {
               uint32_t duration = (uint32_t)((uintptr_t)req.payload);
               handleBleScanStart(duration);
            }
            break;
        case SysCommand::BLE_SCAN_STOP:
            handleBleScanStop();
            break;
        case SysCommand::ACTION_START:
            handleActionStart((ActionRequest*)req.payload);
            break;
        case SysCommand::ACTION_STOP:
            handleActionStop();
            break;
        case SysCommand::SYSTEM_SHUTDOWN:
            RadioWarden::getInstance().releaseRadio();
            break;
        default:
            break;
    }
}

void SystemTask::handleWiFiScanStart() {
    if (Serial) Serial.println("[SYSTEM] Starting WiFi scan");
    
    // Wire up callback to send event back to UI
    BruceWiFi::getInstance().onScanComplete([this](int count) {
        sendEvent(SysEventType::WIFI_SCAN_COMPLETE, (void*)(intptr_t)count, 0, false);
    });
    
    // Wire up Association callback
    BruceWiFi::getInstance().onAssociation([this](const uint8_t* client, const uint8_t* bssid) {
        AssociationEvent* evt = new AssociationEvent();
        memcpy(evt->bssid, bssid, 6);
        memcpy(evt->station, client, 6);
        sendEvent(SysEventType::ASSOCIATION_FOUND, evt, sizeof(AssociationEvent), true);
    });
    
    BruceWiFi::getInstance().beginScan();
    sendEvent(SysEventType::WIFI_SCAN_STARTED);
}

void SystemTask::handleWiFiScanStop() {
    BruceWiFi::getInstance().stopScan();
}

void SystemTask::handleBleScanStart(uint32_t duration) {
    if (Serial) Serial.printf("[SYSTEM] Starting BLE scan (%ums)\n", duration);
    
    BruceBLE::getInstance().onDeviceFound([this](const BLEDeviceInfo& device) {
        BLEDeviceInfo* copy = new BLEDeviceInfo(device);
        if (!copy) { Serial.println("[SYSTEM] ERR: Heap allocation failed"); return; }
        sendEvent(SysEventType::BLE_DEVICE_FOUND, copy, sizeof(BLEDeviceInfo), true);
    });
    
    BruceBLE::getInstance().onScanComplete([this](int count) {
        sendEvent(SysEventType::BLE_SCAN_COMPLETE, (void*)(intptr_t)count, 0, false);
    });
    
    BruceBLE::getInstance().beginScan(duration);
    sendEvent(SysEventType::BLE_SCAN_STARTED);
}

void SystemTask::handleBleScanStop() {
    BruceBLE::getInstance().stopScan();
}

void SystemTask::handleActionStart(ActionRequest* req) {
    if (!req) return;

    ActionType type = req->type;
    Target& t = req->target;

    if (Serial) Serial.printf("[SYSTEM] Action start: %d\n", (int)type);

    // IR actions complete instantly, skip m_actionActive
    if (type == ActionType::IR_REPLAY || type == ActionType::IR_TVBGONE) {
        startIRAction(type);
        return;
    }

    bool success = false;

    switch (type) {
        case ActionType::DEAUTH_SINGLE:
        case ActionType::DEAUTH_ALL:
        case ActionType::CAPTURE_HANDSHAKE:
        case ActionType::CAPTURE_PMKID:
        case ActionType::BEACON_FLOOD:
        case ActionType::PROBE_FLOOD:
        case ActionType::MONITOR:
            success = startWiFiAction(type, t, req);
            break;

        case ActionType::BLE_SPAM:
        case ActionType::BLE_SOUR_APPLE:
        case ActionType::BLE_SKIMMER_DETECT:
            success = startBLEAction(type);
            break;

        case ActionType::EVIL_TWIN:
            success = startPortalAction(t);
            break;

        default:
            sendEvent(SysEventType::ERROR_OCCURRED, (void*)"Action not supported", 0, false);
            return;
    }

    if (success) {
        m_actionActive = true;
        m_currentAction = type;
        m_actionStartTime = millis();
        m_lastProgressTime = millis();
    } else {
        m_actionActive = false;
        sendEvent(SysEventType::ERROR_OCCURRED, (void*)"Hardware init failed", 0, false);
    }
}

bool SystemTask::startWiFiAction(ActionType type, Target& t, ActionRequest* req) {
    BruceWiFi& wifi = BruceWiFi::getInstance();
    if (!wifi.init()) return false;

    switch (type) {
        case ActionType::DEAUTH_SINGLE: {
            bool specificClient = false;
            for (int i = 0; i < 6; i++) {
                if (req->stationMac[i] != 0) { specificClient = true; break; }
            }
            return specificClient
                ? wifi.deauthStation(req->stationMac, t.bssid, t.channel)
                : wifi.deauthAll(t.bssid, t.channel);
        }
        case ActionType::DEAUTH_ALL:
            return wifi.deauthAll(t.bssid, t.channel);

        case ActionType::CAPTURE_HANDSHAKE: {
            char filename[64];
            snprintf(filename, sizeof(filename), "/captures/hs_%02X%02X%02X.pcap",
                     t.bssid[3], t.bssid[4], t.bssid[5]);
            wifi.setPcapLogging(true, filename);
            return wifi.captureHandshake(t.bssid, t.channel, true);
        }
        case ActionType::CAPTURE_PMKID: {
            char filename[64];
            snprintf(filename, sizeof(filename), "/captures/pmkid_%02X%02X%02X.pcap",
                     t.bssid[3], t.bssid[4], t.bssid[5]);
            wifi.setPcapLogging(true, filename);
            return wifi.captureHandshake(t.bssid, t.channel, false);
        }
        case ActionType::BEACON_FLOOD: {
            static const char* fakeSSIDs[] = {
                "Free WiFi", "xfinity", "ATT-WiFi", "NETGEAR",
                "linksys", "FBI Van", "Virus.exe", "GetYourOwn"
            };
            return wifi.beaconFlood(fakeSSIDs, 8, t.channel);
        }
        case ActionType::PROBE_FLOOD:
            return wifi.startProbeFlood(t.channel);

        case ActionType::MONITOR:
            return wifi.startMonitor(t.channel);

        default:
            return false;
    }
}

bool SystemTask::startBLEAction(ActionType type) {
    BruceBLE& ble = BruceBLE::getInstance();
    if (!ble.init()) return false;

    switch (type) {
        case ActionType::BLE_SPAM:
            return ble.startSpam(BLESpamType::RANDOM);
        case ActionType::BLE_SOUR_APPLE:
            return ble.startSpam(BLESpamType::SOUR_APPLE);
        case ActionType::BLE_SKIMMER_DETECT:
            ble.beginScan(5000);
            return true;
        default:
            return false;
    }
}

void SystemTask::startIRAction(ActionType type) {
    BruceIR& ir = BruceIR::getInstance();
    if (!ir.init()) return;

    if (type == ActionType::IR_REPLAY) {
        ir.replayLast();
    } else {
        ir.sendTVBGone();
    }

    const char* msg = (type == ActionType::IR_REPLAY) ? "IR signal sent" : "TV-B-Gone complete";

    sendEvent(SysEventType::ACTION_COMPLETE, (void*)(intptr_t)ActionResult::SUCCESS, 0, false);
    ActionProgress* prog = new ActionProgress();
    if (!prog) { Serial.println("[SYSTEM] ERR: Heap allocation failed"); return; }
    prog->type = type;
    prog->result = ActionResult::SUCCESS;
    prog->elapsedMs = 0;
    prog->packetsSent = 1;
    strncpy(prog->statusText, msg, sizeof(prog->statusText) - 1);
    prog->statusText[sizeof(prog->statusText) - 1] = '\0';
    sendEvent(SysEventType::ACTION_PROGRESS, prog, sizeof(ActionProgress), true);
}

bool SystemTask::startPortalAction(Target& t) {
    EvilPortal& portal = EvilPortal::getInstance();
    if (portal.isRunning()) portal.stop();
    return portal.start(t.ssid, t.channel, PortalTemplate::GENERIC_WIFI);
}

void SystemTask::handleActionStop() {
    BruceWiFi::getInstance().stopHardwareActivities();
    BruceBLE::getInstance().stopHardwareActivities();
    m_actionActive = false;
    sendEvent(SysEventType::ACTION_COMPLETE, (void*)(intptr_t)ActionResult::STOPPED, 0, false);
}

} // namespace Vanguard
