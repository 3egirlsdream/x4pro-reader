#pragma once
using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
constexpr int eIncrement = 1;
inline void xTaskNotify(TaskHandle_t, int, int) {}
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void*>(1); }
