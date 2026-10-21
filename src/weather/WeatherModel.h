#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace weather {
constexpr size_t FORECAST_DAYS = 5;
constexpr uint32_t UPDATE_BUDGET_MS = 30000;
constexpr uint16_t STOP_PERCENT = 15;
constexpr uint16_t RESUME_PERCENT = 20;

struct City {
  uint32_t id;
  const char* province;
  const char* name;
  float latitude;
  float longitude;
};

struct Settings {
  uint32_t cityId = 0;
  uint16_t intervalMinutes = 30;
  bool enabled = false;
  bool batteryPaused = false;
};

struct Day {
  int64_t time = 0;
  float minimum = 0;
  float maximum = 0;
  int16_t code = 0;
  int16_t rainPercent = -1;
};

struct Snapshot {
  uint32_t cityId = 0;
  int64_t dataTime = 0;
  int64_t fetchedAt = 0;
  float temperature = 0;
  float wind = 0;
  int16_t humidity = 0;
  int16_t code = 0;
  std::array<Day, FORECAST_DAYS> days{};
  uint8_t dayCount = 0;
};

enum class UpdateResult { Idle, Running, Success, NetworkError, InvalidData, Timeout, Cancelled, ClockError };

// Render-task owned; polling and duplicate invalidations do not produce new frames.
class FrameGate {
  bool painted = false;
  UpdateResult previous = UpdateResult::Idle;
  bool previousPaused = false;

 public:
  bool claim(UpdateResult state, bool paused) {
    if (painted && previous == state && previousPaused == paused) return false;
    painted = true;
    previous = state;
    previousPaused = paused;
    return true;
  }
};

constexpr bool validInterval(uint16_t minutes) { return minutes == 15 || minutes == 30 || minutes == 60; }
constexpr bool expired(uint32_t now, uint32_t start) { return uint32_t(now - start) >= UPDATE_BUDGET_MS; }
constexpr bool pauseForBattery(bool paused, bool known, uint16_t percent, bool userWake) {
  if (!known || percent > 100 || percent <= STOP_PERCENT) return true;
  if (paused) return !(userWake && percent >= RESUME_PERCENT);
  return false;
}
bool validCoordinates(float latitude, float longitude);
uint32_t coordinateLocationId(float latitude, float longitude);
uint32_t coordinateLocationIdE5(int32_t latitude, int32_t longitude);
bool parseCoordinate(const char* text, float minimum, float maximum, float& value);
bool validSnapshot(const Snapshot& value);
bool fresh(const Snapshot& value, const Settings& settings, int64_t now);
}  // namespace weather
