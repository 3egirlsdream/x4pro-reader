#pragma once
#include <algorithm>
namespace weather {
struct FooterButtonRect {
  int x, y, width, height;
};
inline FooterButtonRect footerButtonRect(int width, int height, int sidePadding, bool refresh) {
  const int margin = std::max(16, sidePadding);
  return {refresh ? width / 2 + 8 : margin, height - 56, width / 2 - margin - 8, 50};
}
}  // namespace weather
