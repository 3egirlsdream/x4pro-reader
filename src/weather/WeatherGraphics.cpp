// Adapted from t0nyz0/crosspoint-reader-lockscreens (MIT); see NOTICE-weather.md.
#include "WeatherGraphics.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cmath>

namespace weather::graphics {
namespace {
// 5x7 dot-matrix glyphs for the big stat numbers. Each glyph is 7 rows of 5
// bits, MSB = leftmost column.
struct BigGlyph {
  char ch;
  uint8_t rows[7];
  uint8_t width;  // in dot columns
};
constexpr BigGlyph BIG_GLYPHS[] = {
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}, 5},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, 5},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}, 5},
    {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}, 5},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}, 5},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}, 5},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}, 5},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, 5},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}, 5},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}, 5},
    {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11000, 0b11000}, 2},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}, 5},
    {'k', {0b10000, 0b10000, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010}, 5},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}, 5},
    {'%', {0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011}, 5},
};

const BigGlyph* findBigGlyph(char c) {
  for (const auto& g : BIG_GLYPHS) {
    if (g.ch == c) return &g;
  }
  return nullptr;
}
}  // namespace

int bigTextWidth(const char* text, int dot) {
  int w = 0;
  for (const char* p = text; *p; p++) {
    const BigGlyph* g = findBigGlyph(*p);
    w += ((g ? g->width : 5) + 1) * dot;
  }
  return w > 0 ? w - dot : 0;  // drop trailing inter-glyph space
}

void drawBigText(const GfxRenderer& renderer, int x, int y, const char* text, int dot) {
  for (const char* p = text; *p; p++) {
    const BigGlyph* g = findBigGlyph(*p);
    if (!g) {
      x += 6 * dot;
      continue;
    }
    for (int r = 0; r < 7; r++) {
      for (int c = 0; c < 5; c++) {
        if (g->rows[r] & (1 << (4 - c))) {
          renderer.fillRect(x + c * dot, y + r * dot, dot, dot);
        }
      }
    }
    x += (g->width + 1) * dot;
  }
}

namespace {
// Filled circle via the rounded-rect trick (corner radius = half the side).
void fillCircle(const GfxRenderer& r, int cx, int cy, int rad) {
  if (rad < 1) rad = 1;
  r.fillRoundedRect(cx - rad, cy - rad, rad * 2, rad * 2, rad, Color::Black);
}

// Sun: filled disc + 8 rays at 45-degree steps, all scaled to discR.
void drawSun(const GfxRenderer& r, int cx, int cy, int discR) {
  fillCircle(r, cx, cy, discR);
  const int gap = discR * 45 / 100 + 2;
  const int len = discR * 75 / 100 + 2;
  const int rayW = discR >= 14 ? 3 : 2;
  const int inner = discR + gap;
  const int outer = inner + len;
  for (int a = 0; a < 8; a++) {
    const float ang = static_cast<float>(a) * 3.14159265f / 4.0f;
    const float s = std::sin(ang), c = std::cos(ang);
    r.drawLine(cx + static_cast<int>(s * inner), cy - static_cast<int>(c * inner), cx + static_cast<int>(s * outer),
               cy - static_cast<int>(c * outer), rayW, true);
  }
}

// Classic cloud silhouette (bumpy top, flat bottom) filling the box
// [x, x+w) x [y, y+h). A flat base slab plus three overlapping lumps, so the
// union reads unmistakably as a cloud even as a solid 1-bit shape (rather than
// the single oval it used to be).
void drawCloud(const GfxRenderer& r, int x, int y, int w, int h) {
  // Rounded base slab (rounded bottom corners keep it from looking boxy) plus
  // three overlapping lumps -- big center, lower/wider sides that bulge out.
  const int baseTop = y + h * 46 / 100;
  r.fillRoundedRect(x + w * 12 / 100, baseTop, w * 76 / 100, (y + h) - baseTop, h * 22 / 100, Color::Black);
  fillCircle(r, x + w * 50 / 100, y + h * 34 / 100, h * 32 / 100);  // center (tallest)
  fillCircle(r, x + w * 28 / 100, y + h * 54 / 100, h * 26 / 100);  // left
  fillCircle(r, x + w * 72 / 100, y + h * 52 / 100, h * 27 / 100);  // right
}
}  // namespace

void drawWeatherIcon(const GfxRenderer& renderer, WxCategory category, int x, int y, int size) {
  if (category == WxCategory::Clear) {
    drawSun(renderer, x + size / 2, y + size / 2, size * 30 / 100);
    return;
  }
  if (category == WxCategory::PartlyCloudy) {
    // Small sun peeking from the top-left, cloud overlapping the lower-right.
    drawSun(renderer, x + size * 34 / 100, y + size * 30 / 100, size * 18 / 100);
    drawCloud(renderer, x + size * 18 / 100, y + size * 34 / 100, size * 82 / 100, size * 58 / 100);
    return;
  }

  // Every other condition is a cloud with something beneath it. Reserve the
  // lower part of the box for the precipitation/effect marks.
  const int cloudH = size * 66 / 100;
  drawCloud(renderer, x, y, size, cloudH);
  const int precipTop = y + cloudH + std::max(2, size / 20);

  switch (category) {
    case WxCategory::Rain:
    case WxCategory::Drizzle: {
      const int streak = category == WxCategory::Rain ? size * 20 / 100 : size * 12 / 100;
      for (int i = 0; i < 3; i++) {
        const int px = x + size * 30 / 100 + i * size * 20 / 100;
        renderer.drawLine(px + streak / 3, precipTop, px - streak / 3, precipTop + streak, 2, true);
      }
      break;
    }
    case WxCategory::Snow: {
      const int flakeR = size * 8 / 100;
      for (int i = 0; i < 3; i++) {
        const int px = x + size * 30 / 100 + i * size * 20 / 100;
        const int py = precipTop + flakeR;
        if (flakeR >= 4) {
          // 3-spoke asterisk flake
          for (int a = 0; a < 3; a++) {
            const float ang = static_cast<float>(a) * 3.14159265f / 3.0f;
            const float s = std::sin(ang), c = std::cos(ang);
            renderer.drawLine(px - static_cast<int>(c * flakeR), py - static_cast<int>(s * flakeR),
                              px + static_cast<int>(c * flakeR), py + static_cast<int>(s * flakeR), 1, true);
          }
        } else {
          renderer.fillRect(px - 1, py - 1, 3, 3);
        }
      }
      break;
    }
    case WxCategory::Storm: {
      // Filled lightning bolt (zigzag) centered under the cloud.
      const int cx = x + size / 2;
      const int t = precipTop;
      const int b = precipTop + size * 26 / 100;
      const int m = (t + b) / 2;
      const int wdt = size * 10 / 100 + 2;
      renderer.drawLine(cx + wdt / 2, t, cx - wdt, m, 3, true);
      renderer.drawLine(cx - wdt, m, cx + wdt / 3, m, 3, true);
      renderer.drawLine(cx + wdt / 3, m, cx - wdt, b, 3, true);
      break;
    }
    case WxCategory::Fog: {
      const int n = 3;
      for (int i = 0; i < n; i++) {
        const int inset = (i % 2) * (size * 12 / 100);
        renderer.fillRect(x + size * 12 / 100 + inset, precipTop + i * std::max(3, size / 14), size * 76 / 100 - inset,
                          std::max(2, size / 28));
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace weather::graphics
