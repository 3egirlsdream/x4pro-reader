#pragma once
#include <ctime>
struct FakeClock {
  bool available = false;
  time_t epoch = 0;
  bool setUtc(time_t now) {
    epoch = now;
    return available;
  }
  bool localTime(struct tm& out) const {
    if (!available) return false;
    gmtime_r(&epoch, &out);
    return true;
  }
};
inline FakeClock halClock;
