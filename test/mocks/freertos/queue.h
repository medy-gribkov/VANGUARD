#ifndef MOCK_QUEUE_H
#define MOCK_QUEUE_H

#include <queue>
#include <vector>
#include <cstring>
#include "FreeRTOS.h"

// Simple mock queue implementation
struct MockQueue {
    size_t itemSize;
    std::queue<std::vector<uint8_t>> items;
};

inline QueueHandle_t xQueueCreate(uint32_t len, uint32_t size) {
    MockQueue* q = new MockQueue();
    q->itemSize = size;
    return (QueueHandle_t)q;
}

inline int xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, uint32_t xTicksToWait) {
    MockQueue* q = (MockQueue*)xQueue;
    std::vector<uint8_t> data((uint8_t*)pvItemToQueue, (uint8_t*)pvItemToQueue + q->itemSize);
    q->items.push(data);
    return pdTRUE;
}

inline int xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, uint32_t xTicksToWait) {
    MockQueue* q = (MockQueue*)xQueue;
    if (q->items.empty()) return pdFALSE;
    std::vector<uint8_t> data = q->items.front();
    q->items.pop();
    memcpy(pvBuffer, data.data(), q->itemSize);
    return pdTRUE;
}

#endif
