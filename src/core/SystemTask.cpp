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
    m_lastProgressTime(0)
{
    // Create Queues
    // Request Queue: UI -> System (Capacity 10)
    m_reqQueue = xQueueCreate(10, sizeof(SystemRequest));

    // Event Queue: System -> UI (Capacity 20 - events can be bursty)
    m_evtQueue = xQueueCreate(20, sizeof(SystemEvent));

    if (!m_reqQueue || !m_evtQueue) {
        if (Serial) Serial.println("[System] FATAL: Queue creation failed");
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
    if (Serial) Serial.println("[System] Task started on Core 0");
}

void SystemTask::subscribeWatchdog() {
    // Subscribe this task to the Task WDT so esp_task_wdt_reset() works.
    // Without this, the Core 0 idle task (which IS subscribed) starves
    // because our tight loop never yields long enough for it to run.
    esp_task_wdt_add(NULL);  // NULL = current task
    if (Serial) Serial.println("[System] Subscribed to Task WDT");
}

void SystemTask::taskLoop(void* param) {
    SystemTask* self = (SystemTask*)param;
    self->run();
}

void SystemTask::run() {
    subscribeWatchdog();

    SystemRequest req;

    while (true) {
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
             uint32_t now = millis();
             
             // Check for Action Completion logic
             bool completed = false;
             ActionResult result = ActionResult::SUCCESS;
             const char* statusMsg = nullptr;

             if (m_currentAction == ActionType::CAPTURE_HANDSHAKE) {
                 if (BruceWiFi::getInstance().hasHandshake()) {
                     completed = true;
                     result = ActionResult::SUCCESS;
                     statusMsg = "Handshake Captured!";
                 } else if (now - m_actionStartTime > 60000) { // 60s timeout
                     completed = true;
                     result = ActionResult::FAILED_TIMEOUT;
                     statusMsg = "Capture timed out";
                 }
             }

             if (completed) {
                 m_actionActive = false;
                 sendEvent(SysEventType::ACTION_COMPLETE, (void*)(intptr_t)result, 0, false);
                 ActionProgress* finalProg = new ActionProgress();
                 finalProg->type = m_currentAction;
                 finalProg->startTimeMs = m_actionStartTime;
                 finalProg->elapsedMs = now - m_actionStartTime;
                 finalProg->result = result;
                 finalProg->packetsSent = BruceWiFi::getInstance().getPacketsSent();
                 strncpy(finalProg->statusText, statusMsg ? statusMsg : "", sizeof(finalProg->statusText) - 1);
                 finalProg->statusText[sizeof(finalProg->statusText) - 1] = '\0';
                 sendEvent(SysEventType::ACTION_PROGRESS, finalProg, sizeof(ActionProgress), true);
             } else if (now - m_lastProgressTime > 500) {
                  // Send progress update
                  ActionProgress* prog = new ActionProgress();
                  prog->type = m_currentAction;
                  prog->elapsedMs = now - m_actionStartTime;
                  prog->result = ActionResult::IN_PROGRESS;

                  // Get stats
                  uint32_t wifiPackets = BruceWiFi::getInstance().getPacketsSent();
                  uint32_t blePackets = BruceBLE::getInstance().getAdvertisementsSent();
                  prog->packetsSent = wifiPackets + blePackets;

                  // Contextual status text (safe copy into fixed buffer)
                  if (m_currentAction == ActionType::CAPTURE_HANDSHAKE) {
                      strncpy(prog->statusText, "Sniffing EAPOL...", sizeof(prog->statusText) - 1);
                  } else if (m_currentAction == ActionType::EVIL_TWIN) {
                      snprintf(prog->statusText, sizeof(prog->statusText), "Portal: %d clients",
                               EvilPortal::getInstance().getClientCount());
                  } else {
                      strncpy(prog->statusText, "Attacking...", sizeof(prog->statusText) - 1);
                  }
                  prog->statusText[sizeof(prog->statusText) - 1] = '\0';

                  prog->startTimeMs = m_actionStartTime;
                  sendEvent(SysEventType::ACTION_PROGRESS, prog, sizeof(ActionProgress), true);
                  m_lastProgressTime = now;
             }
        }
        
        // Feed Watchdog for Core 0
        esp_task_wdt_reset();
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
        if (Serial) Serial.println("[System] WARN: Event queue full, dropped event");
    }
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
    if (Serial) Serial.println("[System] Starting WiFi Scan...");
    
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
    if (Serial) Serial.printf("[System] Starting BLE Scan (%ums)...\n", duration);
    
    BruceBLE::getInstance().onDeviceFound([this](const BLEDeviceInfo& device) {
        BLEDeviceInfo* copy = new BLEDeviceInfo(device);
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
    
    if (Serial) Serial.printf("[System] Action Start: %d\n", (int)type);
    
    BruceWiFi& wifi = BruceWiFi::getInstance();
    BruceBLE& ble = BruceBLE::getInstance();
    
    bool success = false;
    
    switch (type) {
        case ActionType::DEAUTH_SINGLE:
             if (wifi.init()) {
                 // Check if stationMac is valid (not all zeros)
                 bool specificClient = false;
                 for (int i=0; i<6; i++) if (req->stationMac[i] != 0) specificClient = true;
                 
                 if (specificClient) {
                     success = wifi.deauthStation(req->stationMac, t.bssid, t.channel);
                 } else {
                     // Fallback to all if no client specified
                     success = wifi.deauthAll(t.bssid, t.channel);
                 }
             }
             break;

        case ActionType::DEAUTH_ALL:
             if (wifi.init()) {
                 success = wifi.deauthAll(t.bssid, t.channel);
             }
             break;

        // ... Beacon/BLE existing ...

        case ActionType::CAPTURE_HANDSHAKE:
             if (wifi.init()) {
                 // 1. Start Capture (BruceWiFi handles PCAP internally if captureHandshake is called)
                 // But we want to ensure it's logged to our specific filename
                 char filename[64];
                 snprintf(filename, sizeof(filename), "/captures/hs_%02X%02X%02X.pcap", 
                          t.bssid[3], t.bssid[4], t.bssid[5]);
                 wifi.setPcapLogging(true, filename);
                 
                 success = wifi.captureHandshake(t.bssid, t.channel, true);
             }
             break;

        case ActionType::EVIL_TWIN:
             {
                 EvilPortal& portal = EvilPortal::getInstance();
                 if (portal.isRunning()) portal.stop();
                 success = portal.start(t.ssid, t.channel, PortalTemplate::GENERIC_WIFI);
             }
             break;

        case ActionType::BEACON_FLOOD:
             if (wifi.init()) {
                 static const char* fakeSSIDs[] = {
                    "Free WiFi", "xfinity", "ATT-WiFi", "NETGEAR",
                    "linksys", "FBI Van", "Virus.exe", "GetYourOwn"
                 };
                 success = wifi.beaconFlood(fakeSSIDs, 8, t.channel);
             }
             break;
             
        case ActionType::BLE_SPAM:
             if (ble.init()) {
                 success = ble.startSpam(BLESpamType::RANDOM);
             }
             break;
             
        case ActionType::BLE_SOUR_APPLE:
             if (ble.init()) {
                 success = ble.startSpam(BLESpamType::SOUR_APPLE);
             }
             break;

        case ActionType::IR_REPLAY:
             {
                 BruceIR& ir = BruceIR::getInstance();
                 if (ir.init()) {
                     ir.replayLast();
                     success = true;
                 }
             }
             break;

        case ActionType::IR_TVBGONE:
             {
                 BruceIR& ir = BruceIR::getInstance();
                 if (ir.init()) {
                     ir.sendTVBGone();
                     success = true;
                 }
             }
             break;

        case ActionType::PROBE_FLOOD:
             if (wifi.init()) {
                 success = wifi.startProbeFlood(t.channel);
             }
             break;

        case ActionType::MONITOR:
             if (wifi.init()) {
                 success = wifi.startMonitor(t.channel);
             }
             break;

        case ActionType::CAPTURE_PMKID:
             if (wifi.init()) {
                 char filename[64];
                 snprintf(filename, sizeof(filename), "/captures/pmkid_%02X%02X%02X.pcap",
                          t.bssid[3], t.bssid[4], t.bssid[5]);
                 wifi.setPcapLogging(true, filename);
                 success = wifi.captureHandshake(t.bssid, t.channel, false); // false = don't send deauths
             }
             break;

        case ActionType::BLE_SKIMMER_DETECT:
             if (ble.init()) {
                 // Run a quick scan, then check for skimmers
                 ble.beginScan(5000);
                 success = true; // Detection happens during scan via isLikelySkimmer()
             }
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
        // Send initial progress or started event?
    } else {
        m_actionActive = false;
        sendEvent(SysEventType::ERROR_OCCURRED, (void*)"Hardware init failed", 0, false);
    }
}

void SystemTask::handleActionStop() {
    BruceWiFi::getInstance().stopHardwareActivities();
    BruceBLE::getInstance().stopHardwareActivities();
    m_actionActive = false;
    sendEvent(SysEventType::ACTION_COMPLETE, (void*)(intptr_t)ActionResult::CANCELLED, 0, false);
}

} // namespace Vanguard
