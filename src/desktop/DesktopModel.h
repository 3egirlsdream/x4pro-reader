#pragma once
#include <algorithm>
#include <cstdint>

namespace desktop {
constexpr int BOOKS_PER_PAGE = 6;
constexpr int APP_COLUMNS = 4;
constexpr int APP_CAPACITY = APP_COLUMNS * 2;
constexpr int APP_BASE = 16;
constexpr int PAGE_PREVIOUS = 30;
constexpr int PAGE_NEXT = 31;
enum class Progress : uint8_t { Unread, Unknown, Reading, Finished };
struct Rank {
  uint64_t stamp = 0;
  uint16_t ordinal = 0;
  uint16_t reserved[3]{};
};
inline bool recentFirst(const Rank& a, const Rank& b) {
  return a.stamp != b.stamp ? a.stamp > b.stamp : a.ordinal < b.ordinal;
}
struct Box {
  int x, y, width, height;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + width && py < y + height; }
};
struct Layout {
  Box books[BOOKS_PER_PAGE]{};
  Box apps[APP_CAPACITY]{};
  Box previous{}, next{}, heading{}, appHeading{}, empty{};
  int coverHeight = 0;
  Layout(int width, int height, int top, int bottom, int side) {
    const int gap = 12;
    const int appHeight = 82;
    const int appTop = height - bottom - 2 * appHeight;
    appHeading = {side, appTop - 32, width - 2 * side, 28};
    const int cellWidth = (width - 2 * side) / APP_COLUMNS;
    for (int i = 0; i < APP_CAPACITY; ++i)
      apps[i] = {side + (i % APP_COLUMNS) * cellWidth, appTop + (i / APP_COLUMNS) * appHeight, cellWidth, appHeight};
    heading = {side, top, width - 2 * side, 36};
    const int shelfTop = top + 40;
    const int shelfBottom = appHeading.y - 56;
    const int bookWidth = (width - 2 * side - 2 * gap) / 3;
    const int rowHeight = std::max(64, (shelfBottom - shelfTop - gap) / 2);
    coverHeight = std::max(24, rowHeight - 66);
    for (int i = 0; i < BOOKS_PER_PAGE; ++i)
      books[i] = {side + (i % 3) * (bookWidth + gap), shelfTop + (i / 3) * (rowHeight + gap), bookWidth, rowHeight};
    previous = {side, shelfBottom + 8, 76, 44};
    next = {width - side - 76, shelfBottom + 8, 76, 44};
    empty = {side, shelfTop, width - 2 * side, shelfBottom - shelfTop};
  }
};
inline int pageCount(int count) { return std::max(1, (count + BOOKS_PER_PAGE - 1) / BOOKS_PER_PAGE); }
// Four system entries are always reachable; remaining slots follow registry order.
inline int homeAppCount(int registered, bool opds) { return std::min(APP_CAPACITY, 4 + registered + int(opds)); }
}  // namespace desktop
