#pragma once
#include <ArduinoJson.h>

#include <cstring>

#include "fontIds.h"
class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  Orientation orientation = Portrait;
  mutable JsonDocument document;
  int getScreenWidth() const { return orientation == Portrait || orientation == PortraitInverted ? 480 : 800; }
  int getScreenHeight() const { return orientation == Portrait || orientation == PortraitInverted ? 800 : 480; }
  Orientation getOrientation() const { return orientation; }
  void setOrientation(Orientation o) { orientation = o; }
  int textSize(int f) const { return f == SMALL_FONT_ID ? 16 : f == UI_12_FONT_ID ? 25 : 21; }
  int getTextWidth(int font, const char* text) const {
    int units = 0;
    for (auto p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
      if (*p < 128)
        ++units;
      else if ((*p & 0xc0) != 0x80)
        units += 2;
    return units * textSize(font) / 2;
  }
  JsonObject op(const char* kind) const {
    auto o = document["commands"].add<JsonObject>();
    o["op"] = kind;
    return o;
  }
  void clearScreen() const {
    document.clear();
    document["commands"].to<JsonArray>();
  }
  void drawText(int f, int x, int y, const char* text, bool = true) const {
    auto o = op("text");
    o["x"] = x;
    o["y"] = y;
    o["size"] = textSize(f);
    o["text"] = text;
  }
  void drawRect(int x, int y, int w, int h, int lineWidth, bool black) const {
    auto o = op("outline");
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    o["width"] = lineWidth;
    o["black"] = black;
  }
  void drawRect(int x, int y, int w, int h, bool black = true) const { drawRect(x, y, w, h, 1, black); }
  void fillRect(int x, int y, int w, int h, bool black = true) const {
    auto o = op("rect");
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    o["black"] = black;
  }
  void drawPixel(int x, int y, bool black = true) const {
    auto o = op("pixel");
    o["x"] = x;
    o["y"] = y;
    o["black"] = black;
  }
  void drawLine(int x, int y, int x2, int y2, bool black = true) const {
    auto o = op("line");
    o["x"] = x;
    o["y"] = y;
    o["x2"] = x2;
    o["y2"] = y2;
    o["black"] = black;
  }
  void displayBuffer(int mode);
};
