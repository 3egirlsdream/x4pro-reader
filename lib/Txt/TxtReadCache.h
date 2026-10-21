#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Optional read-ahead storage belongs to one open book, never to a global SD handle.
class TxtReadCache {
 public:
  static constexpr size_t CAPACITY = 16 * 1024;
  using Read = bool (*)(void*, uint8_t*, size_t, size_t);
  void reset(uint8_t* storage) {
    bytes = storage;
    length = 0;
    warm = false;
  }
  bool read(Read source, void* context, size_t fileSize, uint8_t* output, size_t offset, size_t count) {
    if (offset > fileSize || count > fileSize - offset || !output) return false;
    if (!count) return true;
    if (!bytes || count > CAPACITY) return source(context, output, offset, count);
    if (offset < base || offset - base > length || count > length - (offset - base)) {
      // Cold open reads only the requested bytes; sequential misses prefetch a bounded window.
      const size_t wanted = warm ? std::min(CAPACITY, fileSize - offset) : count;
      length = 0;
      if (!source(context, bytes, offset, wanted)) return false;
      base = offset;
      length = wanted;
      warm = true;
    }
    memcpy(output, bytes + offset - base, count);
    return true;
  }

 private:
  uint8_t* bytes = nullptr;
  size_t base = 0, length = 0;
  bool warm = false;
};
