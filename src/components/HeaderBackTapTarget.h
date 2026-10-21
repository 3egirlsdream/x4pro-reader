#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

// One 32-bit publication, with no heap or blocking lock on either task.
// Coordinates cover the 800px panel; each target dimension is at most 64px.
namespace HeaderBackTapTarget {
struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int tx, int ty) const { return w > 0 && h > 0 && tx >= x && tx < x + w && ty >= y && ty < y + h; }
};
inline std::atomic<uint32_t> published{0};
inline uint32_t staged = 0;  // render task only, protected by RenderLock
inline uint32_t encode(Rect r) {
  if (r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0 || r.x > 1023 || r.y > 1023) return 0;
  return uint32_t(r.x) | (uint32_t(r.y) << 10) | (uint32_t(std::min(r.w, 64) - 1) << 20) |
         (uint32_t(std::min(r.h, 64) - 1) << 26);
}
inline Rect decode(uint32_t value) {
  if (!value) return {};
  return {int(value & 1023), int((value >> 10) & 1023), int((value >> 20) & 63) + 1, int((value >> 26) & 63) + 1};
}
// Grow around the actual painted square, clipped to the header's content row.
// Never extend into status chrome, list rows, or the opposite action buttons.
inline Rect geometry(int bandX, int bandY, int bandW, int bandH, int strip, int buttonSize, int actionOffset) {
  const int left = std::max(bandX, bandX + 4 + (buttonSize - 64) / 2);
  const int top = std::max(bandY + strip, bandY + 4 + actionOffset + (buttonSize - 64) / 2);
  const int right = std::min(bandX + bandW, left + 64);
  const int bottom = std::min(bandY + bandH, top + 64);
  return {left, top, right - left, bottom - top};
}
inline void beginFrame() { staged = 0; }
inline void set(Rect rect) { staged = encode(rect); }
inline void hide() { staged = 0; }
inline void publish() { published.store(staged); }
inline uint32_t snapshot() { return published.load(); }
inline void clear() { published.store(0); }
inline bool contains(int x, int y) { return decode(snapshot()).contains(x, y); }
}  // namespace HeaderBackTapTarget
