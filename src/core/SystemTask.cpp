#include "SystemTask.h"
#include "RadioWarden.h"
#include "../adapters/BruceWiFi.h"
#include "../adapters/BruceBLE.h"
#include "../adapters/BruceIR.h"

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
}

void SystemTask::start() {
    if (m_running) return;
    
    // Start task on Core 0
    // Stack size: 8192 bytes (WiFi/BLE stacks are heavy)
    xTaskCreatePinnedToCore(
        taskLoop,
        "SystemTask",
        8192,
        this,
        1,              // Priority 1 (Low-ish, let default tasks run)
        &m_taskHandle,
        0               // Core 0
    );
    
    m_running = true;
    if (Serial) Serial.println("[System] Task started on Core 0");
}

void SystemTask::taskLoop(void* param) {
    SystemTask* self = (SystemTask*)param;
    self->run();
}

void SystemTask::run() {
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
            if (now - m_lastProgressTime > 500) {
                 // Send progress update
                 // Can't send full struct easily unless we allocate it
                 // Let's send a notification code for now, or use a static/shared buffer approach?
                 // Safer: Allocate ActionProgress
                 ActionProgress* prog = new ActionProgress();
                 prog->type = m_currentAction;
                 prog->elapsedMs = now - m_actionStartTime;
                 prog->result = ActionResult::IN_PROGRESS;
                 
                 // Get stats
                 uint32_t wifiPackets = BruceWiFi::getInstance().getPacketsSent();
                 uint32_t blePackets = BruceBLE::getInstance().getAdvertisementsSent();
                 prog->packetsSent = wifiPackets + blePackets;
                 
                 // Text status (optional, maybe simplify)
                 // Keeping it null for now to save bandwidth/logic
                 prog->statusText = nullptr; 
                 
                 sendEvent(SysEventType::ACTION_PROGRESS, prog, sizeof(ActionProgress), true);
                 m_lastProgressTime = now;
            }
        }
        
        // Feed Watchdog for Core 0 (if enabled)
        // esp_task_wdt_reset();
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
    
    xQueueSend(m_evtQueue, &evt, 0);
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
        case ActionType::DEAUTH_ALL:
             if (wifi.init()) {
                 success = wifi.deauthAll(t.bssid, t.channel);
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
