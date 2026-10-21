#pragma once
#include <GfxRenderer.h>
struct Rect {
  int x, y, width, height;
};
struct ThemeMetrics {
  int topPadding = 5;
  int headerHeight = 84;
  int contentSidePadding = 20;
};
struct PreviewTheme {
  void drawHeader(const GfxRenderer& r, Rect, const char* title, const char*, bool) const {
    r.drawText(UI_12_FONT_ID, 20, 28, title);
    r.drawText(SMALL_FONT_ID, 645, 28, "X4 Pro");
  }
};
inline PreviewTheme previewTheme;
#define GUI previewTheme
class UITheme {
 public:
  ThemeMetrics metrics;
  static UITheme& getInstance() {
    static UITheme value;
    return value;
  }
  const ThemeMetrics& getMetrics() const { return metrics; }
  static void drawCenteredText(const GfxRenderer& r, Rect bounds, int font, int y, const char* text) {
    r.drawText(font, bounds.x + (bounds.width - r.getTextWidth(font, text)) / 2, y, text);
  }
};
