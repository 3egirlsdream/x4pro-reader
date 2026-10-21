#pragma once

#include <cstdint>
#define CROSSPOINT_VECTOR_FONTS 1
class TtfEpdFont {
 public:
  int clears = 0, releases = 0, warms = 0;
  uint8_t lastStyle = 0;
  void clearCache() { ++clears; }
  void releaseResidentCaches() { ++releases; }
  bool addCoverage(const char*, uint8_t style) {
    ++warms;
    lastStyle = style;
    return true;
  }
};
