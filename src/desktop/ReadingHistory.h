#pragma once
#include <cstdint>
#include <string>
namespace desktop {
struct ReadingStamp {
  uint64_t sequence = 0;
  int16_t percent = -1;
  bool finished = false;
};
ReadingStamp readStamp(const std::string& path);
void rememberRead(const std::string& path, int percent, bool finished);
std::string cachePathFor(const std::string& path);
}  // namespace desktop
