#include "SystemMonitor.h"
#include <esp_heap_caps.h>

namespace Vanguard {

SystemMonitor& SystemMonitor::getInstance() {
    static SystemMonitor instance;
    return instance;
}

SystemMonitor::SystemMonitor() : m_taskHandle(nullptr), m_running(false) {
    memset(&m_status, 0, sizeof(SystemStatus));
}

SystemMonitor::~SystemMonitor() {
    stop();
}

void SystemMonitor::start() {
    if (m_running) return;

    m_running = true;
    // Core 0, Low Priority (1) - Monitoring shouldn't block real work
    xTaskCreatePinnedToCore(
        monitorTask,
        "SysMon",
        4096,
        this,
        1,
        &m_taskHandle,
        0
    );
    
    if (Serial) Serial.println("[SysMon] Started on Core 0");
}

void SystemMonitor::stop() {
    m_running = false;
    if (m_taskHandle) {
        vTaskDelete(m_taskHandle);
        m_taskHandle = nullptr;
    }
}

SystemStatus SystemMonitor::getStatus() const {
    if (!m_running) {
        // Refresh on demand if task isn't running
        portENTER_CRITICAL(&m_statusMux);
        m_status.freeHeap = esp_get_free_heap_size();
        m_status.minFreeHeap = esp_get_minimum_free_heap_size();
        m_status.maxAllocHeap = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        m_status.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        m_status.uptime = millis() / 1000;
        portEXIT_CRITICAL(&m_statusMux);
    }
    // Copy under lock to avoid torn reads from Core 0 writer
    portENTER_CRITICAL(&m_statusMux);
    SystemStatus copy = m_status;
    portEXIT_CRITICAL(&m_statusMux);
    return copy;
}

void SystemMonitor::logStatus() {
    SystemStatus s = getStatus(); // Will trigger refresh if not running
    if (Serial) {
        Serial.printf("[SysMon] Heap: %u | Min: %u | MaxBlk: %u | PSRAM: %u | Up: %us\n",
            s.freeHeap, s.minFreeHeap, s.maxAllocHeap, s.freePsram, s.uptime);
    }
}

void SystemMonitor::monitorTask(void* param) {
    SystemMonitor* self = (SystemMonitor*)param;
    
    while (self->m_running) {
        // Gather stats outside lock, then write under lock
        uint32_t freeHeap = esp_get_free_heap_size();
        uint32_t minFree = esp_get_minimum_free_heap_size();
        uint32_t maxAlloc = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        uint32_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        uint32_t uptime = millis() / 1000;

        portENTER_CRITICAL(&self->m_statusMux);
        self->m_status.freeHeap = freeHeap;
        self->m_status.minFreeHeap = minFree;
        self->m_status.maxAllocHeap = maxAlloc;
        self->m_status.freePsram = freePsram;
        self->m_status.uptime = uptime;
        portEXIT_CRITICAL(&self->m_statusMux);

        // Log if changes significant or periodical? 
        // For now, quiet background update. Can un-comment logging for debug.
        // self->logStatus();

        // Check for critical low memory (e.g. < 20KB internal)
        if (self->m_status.maxAllocHeap < 20480) {
            if (Serial) Serial.println("[SysMon] WARNING: Low Memory Fragmentation!");
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Check every 5s
    }
    
    // Self-delete if loop exits (shouldn't)
    self->m_taskHandle = nullptr;
    vTaskDelete(NULL);
}

} // namespace Vanguard
