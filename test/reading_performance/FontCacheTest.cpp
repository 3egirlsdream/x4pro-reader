#include <FontCacheManager.h>
#include <FontReadCache.h>
#include <TtfEpdFont.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

struct Stream {
  std::vector<unsigned char> bytes;
  size_t reads = 0;
  size_t diskReads = 0, diskBytes = 0;
  unsigned long prefix = 1024 * 1024;
  bool shortNext = false;
  FontReadCache cache;
  std::array<unsigned char, FontReadCache::STORAGE_BYTES> cached{};
  static unsigned long disk(void* ctx, unsigned long offset, unsigned char* out, unsigned long count) {
    auto& self = *static_cast<Stream*>(ctx);
    ++self.diskReads;
    if (self.shortNext) {
      self.shortNext = false;
      count = std::min<unsigned long>(count, 16);
    }
    self.diskBytes += count;
    memcpy(out, self.bytes.data() + offset, count);
    return count;
  }
  static unsigned long read(void* ctx, unsigned long offset, unsigned char* out, unsigned long count) {
    auto& self = *static_cast<Stream*>(ctx);
    if (count == 0) return offset <= self.bytes.size() ? 0 : 1;
    if (offset > self.bytes.size() || count > self.bytes.size() - offset) return 0;
    ++self.reads;
    // Match the existing resident 1 MB prefix before testing the new tail cache.
    const auto prefix = std::min<unsigned long>(self.prefix, self.bytes.size());
    const auto head = offset < prefix ? std::min<unsigned long>(count, prefix - offset) : 0;
    if (head) memcpy(out, self.bytes.data() + offset, head);
    if (head == count) return count;
    return head + self.cache.read(disk, ctx, self.bytes.size(), offset + head, out + head, count - head);
  }
};
uint32_t fingerprint(const EpdFontFamily& family) {
  uint32_t hash = 2166136261u;
  for (uint32_t cp : {0x5b8b, 0x6c5f, 0x6b66, 0x677e, 0x201c, 0x2014}) {
    const auto* glyph = family.getGlyph(cp);
    assert(glyph && glyph->dataLength);
    hash = (hash ^ glyph->advanceX) * 16777619u;
    hash = (hash ^ glyph->width) * 16777619u;
    hash = (hash ^ glyph->height) * 16777619u;
    const auto* data = family.getData();
    const auto* bitmap = data->vectorBitmapHandler(data->glyphMissCtx, glyph);
    for (size_t i = 0; i < glyph->dataLength; ++i) hash = (hash ^ bitmap[i]) * 16777619u;
  }
  return hash;
}
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  assert(file);
  Stream stream;
  stream.bytes.assign(std::istreambuf_iterator<char>(file), {});
  TtfEpdFont font;
  font.addStreamSource(0, Stream::read, &stream, stream.bytes.size());
  // Compare cold first-page reads using the same font, hinting and prefix.
  const char* text = "宋江说道：“武松——兄弟！”一二三四五六七八九十，天地玄黄。";
  uint32_t reference = 0;
  {
    TtfEpdFont baseline;
    baseline.addStreamSource(0, Stream::read, &stream, stream.bytes.size());
    assert(baseline.load(14, true, 1024 * 1024, 4096));
    baseline.addCoverage(text);
    reference = fingerprint(baseline.family());
  }
  const size_t uncachedReads = stream.diskReads, uncachedBytes = stream.diskBytes;
  const size_t oldPrefix = stream.prefix;
  stream.prefix = FontReadCache::prefixSize(Stream::disk, &stream, stream.bytes.size());
  assert(stream.prefix >= 267628 && stream.prefix < 1024 * 1024);
  stream.diskReads = stream.diskBytes = stream.reads = 0;
  stream.cache.reset(stream.cached.data());
  assert(font.load(14, true, 1024 * 1024, 4096));
  const std::map<int, EpdFontFamily> families{{7, font.family()}};
  const std::map<int, SdCardFont*> sd;
  const std::map<int, TtfEpdFont*> vectors{{7, &font}};
  FontCacheManager manager(families, sd, vectors);
  const auto render = [&](EpdFontFamily::Style style) {
    auto scope = manager.createPrewarmScope();
    manager.recordText(text, 7, style);
    scope.endScanAndPrewarm();
    assert(families.at(7).getGlyph(0x5b8b, style)->dataLength > 0);
  };
  render(EpdFontFamily::REGULAR);
  assert(fingerprint(font.family()) == reference);
  printf(
      "Cold font outline I/O: prefix-only reads=%zu bytes=%zu; prefix+8KB-block-cache reads=%zu bytes=%zu (host "
      "callback counts, not hardware timing)\n",
      uncachedReads, uncachedBytes, stream.diskReads, stream.diskBytes);
  assert(stream.diskReads < uncachedReads);
  printf("Font prefix cold boot bytes: %zu -> %lu; prefix+outline bytes: %zu -> %zu\n", oldPrefix, stream.prefix,
         oldPrefix + uncachedBytes, stream.prefix + stream.diskBytes);
  const size_t coldReads = stream.reads;
  for (int i = 0; i < 50; ++i) render(EpdFontFamily::REGULAR);
  assert(stream.reads == coldReads);
  assert(fingerprint(font.family()) == reference);
  render(EpdFontFamily::BOLD);
  const size_t boldReads = stream.reads;
  for (int i = 0; i < 10; ++i) render(EpdFontFamily::BOLD);
  assert(stream.reads == boldReads);
  manager.clearCache();
  render(EpdFontFamily::REGULAR);
  assert(stream.reads > boldReads);
  manager.releaseSdFontCaches();
  const size_t beforeReleaseRender = stream.reads;
  render(EpdFontFamily::REGULAR);
  assert(stream.reads > beforeReleaseRender);
  // Resize invalidates cached metrics and glyphs.
  const int advance14 = families.at(7).getGlyph(0x5b8b)->advanceX;
  assert(font.load(18, true, 1024 * 1024, 4096));
  assert(font.family().getGlyph(0x5b8b)->advanceX > advance14);
  // Arbitrary ranges, EOF, seek probes, overflow, disabled cache and reset.
  for (unsigned long i = 0; i < 500; ++i) {
    unsigned char actual[137];
    const auto offset = (i * 9973UL) % (stream.bytes.size() - sizeof(actual));
    assert(stream.cache.read(Stream::disk, &stream, stream.bytes.size(), offset, actual, sizeof(actual)) ==
           sizeof(actual));
    assert(memcmp(actual, stream.bytes.data() + offset, sizeof(actual)) == 0);
  }
  assert(stream.cache.read(Stream::disk, &stream, stream.bytes.size(), stream.bytes.size(), nullptr, 0) == 0);
  assert(stream.cache.read(Stream::disk, &stream, stream.bytes.size(), std::numeric_limits<unsigned long>::max(),
                           nullptr, 2) == 0);
  stream.cache.reset(stream.cached.data());
  stream.shortNext = true;
  unsigned char retry[32];
  assert(stream.cache.read(Stream::disk, &stream, stream.bytes.size(), 512, retry, sizeof(retry)) == 0);
  assert(stream.cache.read(Stream::disk, &stream, stream.bytes.size(), 512, retry, sizeof(retry)) == sizeof(retry));
  assert(!memcmp(retry, stream.bytes.data() + 512, sizeof(retry)));
  const auto saved4 = stream.bytes[4], saved5 = stream.bytes[5];
  stream.bytes[4] = stream.bytes[5] = 255;
  assert(FontReadCache::prefixSize(Stream::disk, &stream, stream.bytes.size()) == 1024 * 1024);
  stream.bytes[4] = saved4;
  stream.bytes[5] = saved5;
  stream.cache.reset(nullptr);
  printf(
      "Real streamed TTF + production page scopes: cold reads=%zu; 50 warm regular pages=0 extra reads; bold "
      "retention, explicit eviction, release and resize passed\n",
      coldReads);
}
