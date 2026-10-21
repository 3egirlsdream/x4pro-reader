#pragma once
#include <cstdint>
struct HalPowerManager {
  bool known = true;
  uint16_t percent = 80;
  bool readBatteryPercentageChecked(uint16_t& out) const {
    out = percent;
    return known;
  }
};
extern HalPowerManager powerManager;
