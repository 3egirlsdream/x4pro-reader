#pragma once
#include <cstdint>
inline uint32_t mockMicros = 100, mockTick = 1;
inline uint32_t micros() {
  mockMicros += mockTick;
  return mockMicros;
}
inline uint32_t millis() { return mockMicros / 1000; }
