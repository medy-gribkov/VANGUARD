#ifndef VANGUARD_SYSTEM_MONITOR_H
#define VANGUARD_SYSTEM_MONITOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Vanguard {

struct SystemStatus {
    uint32_t freeHeap;       // Internal free
    uint32_t minFreeHeap;    // Watermark
    uint32_t maxAllocHeap;   // Largest free block (internal)
    uint32_t freePsram;      // SPIRAM free
    uint32_t uptime;         // Seconds
};

class SystemMonitor {
public:
    static SystemMonitor& getInstance();

    void start();
    void stop();
    
    SystemStatus getStatus() const;
    void logStatus();

private:
    SystemMonitor();
    ~SystemMonitor();

    static void monitorTask(void* param);
    
    TaskHandle_t m_taskHandle;
    bool m_running;
    mutable SystemStatus m_status;
};

} // namespace Vanguard

#endif // VANGUARD_SYSTEM_MONITOR_H
