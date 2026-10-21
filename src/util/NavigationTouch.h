#pragma once

#include <cstdint>
#include <cstdlib>

#include "components/HeaderBackTapTarget.h"

// Loop-task contact ownership. Repaints preserve a contact; navigation resets
// it. A screen action is always a short Back regardless of physical hold time.
class NavigationTouch {
 public:
  static constexpr int SLOP = 28;
  static constexpr unsigned long MAX_TAP_MS = 1200;
  void begin(int x, int y, int orientation, uint32_t target) {
    active = true;
    cancelled = false;
    startX = x;
    startY = y;
    rotation = orientation;
    header = target;
    ownsHeader = HeaderBackTapTarget::decode(header).contains(x, y);
  }
  void move(int x, int y, int orientation) {
    if (active && (orientation != rotation || std::abs(x - startX) > SLOP || std::abs(y - startY) > SLOP))
      cancelled = true;
  }
  void release(bool sdkTap, unsigned long duration, int orientation, uint32_t target) {
    tap = active && !cancelled && sdkTap && duration < MAX_TAP_MS && rotation == orientation;
    back = tap && ownsHeader && header != 0 && header == target;
    active = false;
  }
  void nextFrame() {
    tap = back = false;
    if (!active) ownsHeader = false;
  }
  void reset() { *this = NavigationTouch{}; }
  bool inRect(int x, int y, int w, int h) const {
    return tap && !ownsHeader && startX >= x && startX < x + w && startY >= y && startY < y + h;
  }
  bool active = false, cancelled = false, ownsHeader = false, tap = false, back = false;
  int startX = 0, startY = 0, rotation = 0;
  uint32_t header = 0;
};
