#pragma once
#include <Arduino.h>

#include <memory>
inline int livePsram = 0;
inline size_t peakPsram = 0;
inline void (*queuedTask)(void*) = nullptr;
inline void* queuedContext = nullptr;
inline constexpr int pdPASS = 1;
inline int xTaskCreate(void (*task)(void*), const char*, int, void* context, int, void*) {
  queuedTask = task;
  queuedContext = context;
  return pdPASS;
}
inline void vTaskDelete(void*) {}
inline void runQueuedTask() {
  auto fn = queuedTask;
  queuedTask = nullptr;
  if (fn) fn(queuedContext);
}
class HalMemory {
 public:
  struct Deleter {
    void operator()(uint8_t* p) const {
      if (p) {
        --livePsram;
        delete[] p;
      }
    }
  };
  using PsramBuffer = std::unique_ptr<uint8_t[], Deleter>;
  static PsramBuffer allocatePsram(size_t n) {
    if (fake::fail(fake::failAlloc)) return {};
    ++livePsram;
    peakPsram = std::max(n, peakPsram);
    return PsramBuffer(new uint8_t[n]);
  }
};
