#ifndef VANGUARD_SYSTEM_TASK_H
#define VANGUARD_SYSTEM_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "IPC.h"
#include "VanguardTypes.h"

namespace Vanguard {

/**
 * @brief Manages the Background System Task (Core 0)
 * 
 * Handles all blocking I/O operations including:
 * - WiFi Scanning & Attacks
 * - BLE Scanning & Spaming
 * - File I/O (Logging)
 */
class SystemTask {
public:
    static SystemTask& getInstance();

    /**
     * @brief Start the RTOS task on Core 0
     */
    void start();

    /**
     * @brief Send a request to the System Task (Non-blocking)
     * @return true if queued successfully
     */
    bool sendRequest(const SystemRequest& req);

    /**
     * @brief Check for events from the System Task (Non-blocking)
     * @return true if event retrieved
     */
    bool receiveEvent(SystemEvent& evt);

private:
    SystemTask();
    
    // Task Handle
    TaskHandle_t m_taskHandle;
    
    // IPC Queues
    QueueHandle_t m_reqQueue;
    QueueHandle_t m_evtQueue;
    
    // Task Loop
    static void taskLoop(void* param);
    void run();
    
    // Handlers
    void handleRequest(const SystemRequest& req);
    void handleWiFiScanStart();
    void handleWiFiScanStop();
    void handleBleScanStart(uint32_t duration);
    void handleBleScanStop();
    void handleActionStart(ActionRequest* req);
    void handleActionStop();
    
    // Helpers
    void sendEvent(SysEventType type, void* data = nullptr, size_t len = 0, bool isPtr = false);
    
    // State
    bool m_running;
    bool m_actionActive;
    ActionType m_currentAction;
    uint32_t m_actionStartTime;
    uint32_t m_lastProgressTime;
};

} // namespace Vanguard

#endif // VANGUARD_SYSTEM_TASK_H
