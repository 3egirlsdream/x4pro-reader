#pragma once
#include <FreeInkApp.h>
#include <GfxRenderer.h>

#include <cstring>
namespace freeink::ui {
class GfxRendererTarget : public DrawTarget {
 public:
  const GfxRenderer& renderer;
  Rect lastBitmap{};
  enum { FONT_SMALL, FONT_BODY, FONT_TITLE, FONT_LABEL };
  explicit GfxRendererTarget(const GfxRenderer& r, bool = true) : renderer(r) {}
  DeviceContext deviceContext() const {
    DeviceContext d;
    d.width = renderer.getScreenWidth();
    d.height = renderer.getScreenHeight();
    d.hasTouch = true;
    return d;
  }
  Size measureText(FontId, const char* text, TextStyle) const override {
    return {static_cast<int16_t>(text ? strlen(text) * 8 : 0), 32};
  }
  int16_t lineHeight(FontId) const override { return 32; }
  void fill(Rect, Paint, uint8_t, uint8_t) override {}
  void stroke(Rect, Paint, uint8_t, uint8_t, uint8_t) override {}
  void line(Point, Point, uint8_t, Paint) override {}
  void triangle(Point, Point, Point, Paint) override {}
  void text(Rect, const char*, TextStyle) override {}
  void bitmap(Rect rect, BitmapRef, BitmapMode, Paint, Rotation) override { lastBitmap = rect; }
};
}  // namespace freeink::ui
