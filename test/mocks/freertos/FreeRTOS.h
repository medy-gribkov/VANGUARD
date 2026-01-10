#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <cstdint>

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;

#define portTICK_PERIOD_MS 1
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1

#endif
