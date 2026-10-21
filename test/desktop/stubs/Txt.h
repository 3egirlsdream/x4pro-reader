#pragma once
#include <HalStorage.h>

#include <string>
class Txt {
  std::string cache;

 public:
  Txt(const std::string& p, const char*) : cache("/.crosspoint/txt_" + std::to_string(std::hash<std::string>{}(p))) {}
  const auto& getCachePath() const { return cache; }
  void setupCacheDir() {}
  bool generateCoverBmp() const { return false; }
  std::string getCoverBmpPath() const { return cache + "/cover.bmp"; }
};
