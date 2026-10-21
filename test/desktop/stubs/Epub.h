#pragma once
#include <HalStorage.h>

#include <map>
#include <string>
inline int generatedCovers = 0;
inline std::string generatedPath;
struct FakeMetadata {
  std::string title = "Title", author = "Author";
  bool success = true;
};
inline std::map<std::string, FakeMetadata> bookMetadata;
class Epub {
  std::string path, cache;

 public:
  Epub(const std::string& p, const char*)
      : path(p), cache("/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(p))) {}
  bool loadMetadata(std::string& t, std::string& a) {
    ++fake::parses;
    t = bookMetadata[path].title;
    a = bookMetadata[path].author;
    return bookMetadata[path].success;
  }
  bool load(bool, bool) { return false; }
  const auto& getCachePath() const { return cache; }
  int getSpineItemsCount() const { return 3; }
  size_t getBookSize() const { return 1000; }
  float calculateProgress(int spine, float fraction) const { return (spine + fraction) / 3; }
  bool generateThumbBmpFromSource(int height) {
    ++generatedCovers;
    generatedPath = path;
    fake::add(getThumbBmpPath(height));
    return true;
  }
  std::string getThumbBmpPath(int h) const { return cache + "/thumb_" + std::to_string(h) + ".bmp"; }
};
