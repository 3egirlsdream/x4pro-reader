#pragma once
#include <ArduinoJson.h>

#include <algorithm>
#include <cstring>
#include <iostream>

#include "fontIds.h"
enum class Color { Black, White };
class GfxRenderer {
 public:
  mutable JsonDocument document;
  int getScreenWidth() const { return 800; }
  int getScreenHeight() const { return 480; }
  int textSize(int font) const { return font == SMALL_FONT_ID ? 16 : font == UI_12_FONT_ID ? 25 : 21; }
  void clearScreen() const {
    document.clear();
    document["commands"].to<JsonArray>();
  }
  JsonObject op(const char* kind) const {
    auto obj = document["commands"].add<JsonObject>();
    obj["op"] = kind;
    return obj;
  }
  void fillRect(int x, int y, int w, int h, bool black = true) const {
    auto o = op("rect");
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    o["fill"] = black;
  }
  void drawRect(int x, int y, int w, int h, bool = true) const {
    auto o = op("outline");
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
  }
  void fillRoundedRect(int x, int y, int w, int h, int radius, Color color) const {
    auto o = op("rounded");
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    o["r"] = radius;
    o["fill"] = color == Color::Black;
  }
  void drawLine(int x1, int y1, int x2, int y2, int width, bool = true) const {
    auto o = op("line");
    o["x"] = x1;
    o["y"] = y1;
    o["x2"] = x2;
    o["y2"] = y2;
    o["width"] = width;
  }
  void drawLine(int x1, int y1, int x2, int y2, bool black = true) const { drawLine(x1, y1, x2, y2, 1, black); }
  void drawText(int font, int x, int y, const char* text, bool = true) const {
    auto o = op("text");
    o["x"] = x;
    o["y"] = y;
    o["size"] = textSize(font);
    o["text"] = text;
  }
  int getTextWidth(int font, const char* text) const {
    int units = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
      if (*p < 128)
        units += 1;
      else if ((*p & 0xc0) != 0x80)
        units += 2;
    }
    return units * textSize(font) / 2;
  }
};
