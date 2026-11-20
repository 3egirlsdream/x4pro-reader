#pragma once
#include <GfxRenderer.h>
#include <Logging.h>
class RenderLock {
 public:
  enum class Mode { Blocking, Try };
  inline static bool busy = false;
  explicit RenderLock(Mode = Mode::Blocking) {}
  bool ownsLock() const { return !busy; }
  ~RenderLock() {}
};
class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down };
  enum class SwipeDir { None, Left, Right, Up, Down };
  int released = -1;
  bool longDown = false, longUp = false, longTouch = false, tapped = false;
  int tx = 0, ty = 0;
  SwipeDir swipe = SwipeDir::None;
  bool wasReleased(Button b) const { return released == int(b); }
  bool wasLongPressed(Button b, unsigned) const {
    return (longDown && b == Button::Down) || (longUp && b == Button::Up);
  }
  bool wasScreenTapped(int& x, int& y) const {
    x = tx;
    y = ty;
    return tapped;
  }
  bool wasScreenLongPress(int& x, int& y) const {
    x = tx;
    y = ty;
    return longTouch;
  }
  SwipeDir wasSwipe() const { return swipe; }
  void clear() {
    released = -1;
    longDown = longUp = longTouch = tapped = false;
    swipe = SwipeDir::None;
  }
};
class Activity {
 public:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  unsigned requests = 0, renderedRequests = 0;
  Activity(const char*, GfxRenderer& r, MappedInputManager& i) : renderer(r), mappedInput(i) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool handleForcedRefresh() { return false; }
  virtual bool skipLoopDelay() { return false; }
  void requestUpdate() { ++requests; }
};
