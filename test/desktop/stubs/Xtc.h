#pragma once
#include <atomic>
#include <string>
class Xtc {
  std::string cache;

 public:
  Xtc(const std::string& p, const char*) : cache("/.crosspoint/xtc_" + std::to_string(std::hash<std::string>{}(p))) {}
  bool load() const { return false; }
  bool generateThumbBmp(int, const std::atomic<bool>* = nullptr) const { return false; }
  const auto& getCachePath() const { return cache; }
  std::string getThumbBmpPath(int h) const { return cache + "/thumb_" + std::to_string(h) + ".bmp"; }
  int getPageCount() const { return 10; }
  int calculateProgress(int p) const { return (p + 1) * 10; }
};
