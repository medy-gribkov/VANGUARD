#ifndef MOCK_TASK_H
#define MOCK_TASK_H

#include "FreeRTOS.h"

inline int xTaskCreatePinnedToCore(void (*pvTaskCode)(void *), const char * const pcName, const uint32_t usStackDepth, void * const pvParameters, uint32_t uxPriority, TaskHandle_t * const pxCreatedTask, const int xCoreID) {
    if (pxCreatedTask) *pxCreatedTask = (TaskHandle_t)1;
    return pdPASS;
}

#endif
