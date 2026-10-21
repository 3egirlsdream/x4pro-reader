#pragma once
#include <GfxRenderer.h>
struct MockDisplay {
  enum { FULL_REFRESH = 1, FAST_REFRESH = 2 };
  unsigned frames = 0, full = 0, fast = 0;
  bool inverted = false;
  void setInverted(bool value) { inverted = value; }
};
using HalDisplay = MockDisplay;
inline MockDisplay display;
inline void GfxRenderer::displayBuffer(int mode) {
  ++display.frames;
  if (mode == HalDisplay::FULL_REFRESH)
    ++display.full;
  else
    ++display.fast;
}
