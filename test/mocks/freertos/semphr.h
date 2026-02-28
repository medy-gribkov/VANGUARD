#ifndef MOCK_SEMPHR_H
#define MOCK_SEMPHR_H

#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline int xSemaphoreTake(SemaphoreHandle_t, uint32_t) { return pdTRUE; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline uint32_t pdMS_TO_TICKS(uint32_t ms) { return ms; }

// portMUX types (ESP32 spinlock mock)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) (void)(mux)
#define portEXIT_CRITICAL(mux) (void)(mux)

#endif
