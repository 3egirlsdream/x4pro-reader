#pragma once
#include <cstdint>
inline unsigned long hostMillis = 0;
inline unsigned long millis() { return hostMillis; }
class HalGPIO {
 public:
  enum { BTN_BACK, BTN_CONFIRM, BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_POWER };
  bool down = false, held = false, released = false, tap = false, suppressed = false, swipe = false;
  float nx = 0, ny = 0, endX = 0, endY = 0;
  unsigned long duration = 0;
  uint32_t keys = 0, pressedKeys = 0, releasedKeys = 0;
  void update() {}
  bool hasTouch() const { return true; }
  bool hasHomeKey() const { return false; }
  bool wasHomeKeyTapped() const { return false; }
  bool wasHomeKeyLongPressed() const { return false; }
  bool wasHomeKeyPressed() const { return false; }
  bool isPressed(uint8_t b) const { return keys & (1u << b); }
  bool wasPressed(uint8_t b) const { return pressedKeys & (1u << b); }
  bool wasReleased(uint8_t b) const { return releasedKeys & (1u << b); }
  bool wasAnyPressed() const { return pressedKeys; }
  bool wasAnyReleased() const { return releasedKeys; }
  unsigned long getHeldTime() const { return duration; }
  unsigned long getPowerButtonHeldTime() const { return duration; }
  unsigned long lastTouchHeldMs() const { return duration; }
  bool point(bool present, float& x, float& y) const {
    x = nx;
    y = ny;
    return present && !suppressed;
  }
  bool wasTouchTap(float& x, float& y) const { return point(tap && released, x, y); }
  bool wasTouchDown(float& x, float& y) const { return point(down, x, y); }
  bool isTouchTapCandidate(float& x, float& y, unsigned long& ms) const {
    ms = duration;
    return point(held, x, y);
  }
  bool isTouchHeldAt(float& x, float& y) const { return point(held, x, y); }
  bool wasTouchReleased() const { return released && !suppressed; }
  bool wasTouchLongPress(float&, float&) const { return false; }
  void suppressTouchContact() { suppressed = held || released; }
  bool wasSwipe(float& x, float& y, float& ex, float& ey) const {
    x = nx;
    y = ny;
    ex = endX;
    ey = endY;
    return swipe && !suppressed;
  }
};
