#pragma once

#include <Arduino.h>

#include <atomic>
#include <cstdint>

namespace performance {
enum class Stage : uint8_t {
  Boot,
  FontInit,
  FontEnsure,
  ActivityEnter,
  ActivityExit,
  NavigationWait,
  RenderWait,
  ReaderFrame,
  TxtLayout,
  TxtPersist,
  PanelBw,
  PanelGray,
  PanelWait,
  ShelfStop,
  ShelfCover,
  WeatherPoll,
  Count
};
struct Counter {
  std::atomic<uint32_t> count{0}, totalMs{0}, maxMs{0}, lastMs{0};
  void add(uint32_t elapsed) {
    count.fetch_add(1, std::memory_order_relaxed);
    totalMs.fetch_add(elapsed, std::memory_order_relaxed);
    lastMs.store(elapsed, std::memory_order_relaxed);
    auto maximum = maxMs.load(std::memory_order_relaxed);
    while (maximum < elapsed && !maxMs.compare_exchange_weak(maximum, elapsed, std::memory_order_relaxed)) {
    }
  }
};
inline Counter counters[static_cast<unsigned>(Stage::Count)];
inline void record(Stage stage, uint32_t elapsed) { counters[static_cast<unsigned>(stage)].add(elapsed); }
class Scope {
 public:
  explicit Scope(Stage stage) : stage(stage), started(millis()) {}
  ~Scope() { record(stage, static_cast<uint32_t>(millis()) - started); }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;

 private:
  Stage stage;
  uint32_t started;
};
// Called only at an activity boundary, while filesystem ownership is local.
bool saveSnapshot();
}  // namespace performance
