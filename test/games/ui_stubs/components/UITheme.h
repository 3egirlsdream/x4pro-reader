#pragma once
#include "components/themes/BaseTheme.h"
struct ThemeMetrics {
  int topPadding = 5, headerHeight = 84;
};
struct PreviewTheme {
  void drawHeader(const GfxRenderer& r, Rect rect, const char* text, const char*, bool) const {
    r.drawText(UI_12_FONT_ID, rect.x + 20, rect.y + 23, text);
  }
};
inline PreviewTheme previewTheme;
#define GUI previewTheme
class UITheme {
 public:
  ThemeMetrics metrics;
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  const ThemeMetrics& getMetrics() const { return metrics; }
  Rect getScreenSafeArea(const GfxRenderer& r, bool, bool) const {
    return {0, 0, r.getScreenWidth(), r.getScreenHeight()};
  }
  static void drawCenteredText(const GfxRenderer& r, Rect bounds, int font, int y, const char* text) {
    r.drawText(font, bounds.x + (bounds.width - r.getTextWidth(font, text)) / 2, y, text);
  }
};
