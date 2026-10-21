#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>

// Shared by the reader and UI faces borrowing one streamed font source.
// Storage is optional, owned by the source, and never allocated from read().
class FontReadCache {
 public:
  static constexpr size_t BLOCK_BYTES = 1024;
  static constexpr size_t BLOCK_COUNT = 8;
  static constexpr size_t STORAGE_BYTES = BLOCK_BYTES * BLOCK_COUNT;
  using Read = unsigned long (*)(void*, unsigned long, unsigned char*, unsigned long);
  static unsigned long prefixSize(Read source, void* ctx, unsigned long size) {
    const auto cap = std::min<unsigned long>(size, 1024 * 1024);
    unsigned char header[12];
    if (size < sizeof(header) || source(ctx, 0, header, sizeof(header)) != sizeof(header)) return cap;
    const bool sfnt = !memcmp(header, "\0\1\0\0", 4) || !memcmp(header, "OTTO", 4) || !memcmp(header, "true", 4);
    const unsigned tables = (unsigned(header[4]) << 8) | header[5];
    if (!sfnt || !tables || tables > 128 || 12 + tables * 16 > size) return cap;
    unsigned long needed = 12 + tables * 16;
    bool metrics = false;
    for (unsigned i = 0; i < tables; ++i) {
      unsigned char table[16];
      if (source(ctx, 12 + i * 16, table, sizeof(table)) != sizeof(table)) return cap;
      if (memcmp(table, "head", 4) && memcmp(table, "hhea", 4) && memcmp(table, "maxp", 4) &&
          memcmp(table, "cmap", 4) && memcmp(table, "loca", 4) && memcmp(table, "hmtx", 4))
        continue;
      const auto be32 = [](const unsigned char* p) {
        return (static_cast<unsigned long>(p[0]) << 24) | (static_cast<unsigned long>(p[1]) << 16) |
               (static_cast<unsigned long>(p[2]) << 8) | p[3];
      };
      const auto offset = be32(table + 8), length = be32(table + 12);
      if (offset > size || length > size - offset) return cap;
      needed = std::max(needed, offset + length);
      if (!memcmp(table, "cmap", 4)) metrics = true;
    }
    if (!metrics || needed > cap) return cap;
    return std::min(cap, ((needed + 4095) / 4096) * 4096);
  }
  void reset(unsigned char* storage) {
    bytes = storage;
    next = 0;
    for (auto& block : blocks) block.length = 0;
  }
  unsigned long read(Read source, void* ctx, unsigned long size, unsigned long offset, unsigned char* out,
                     unsigned long count) {
    if (offset > size) return count ? 0 : 1;
    if (!count) return 0;
    if (count > size - offset || !out) return 0;
    if (!bytes) return source(ctx, offset, out, count);
    unsigned long done = 0;
    while (done < count) {
      const auto position = offset + done;
      const auto base = position - position % BLOCK_BYTES;
      const auto within = position - base;
      size_t slot = 0;
      while (slot < BLOCK_COUNT && (!blocks[slot].length || blocks[slot].offset != base)) ++slot;
      if (slot == BLOCK_COUNT) {
        slot = next;
        next = (next + 1) % BLOCK_COUNT;
        blocks[slot].length = 0;
        const auto wanted = std::min<unsigned long>(BLOCK_BYTES, size - base);
        const auto got = source(ctx, base, bytes + slot * BLOCK_BYTES, wanted);
        if (got != wanted) {
          const auto available = got > within ? std::min<unsigned long>(count - done, got - within) : 0;
          if (available) memcpy(out + done, bytes + slot * BLOCK_BYTES + within, available);
          return done + available;
        }
        blocks[slot] = {base, wanted};
      }
      const auto take = std::min<unsigned long>(count - done, blocks[slot].length - within);
      memcpy(out + done, bytes + slot * BLOCK_BYTES + within, take);
      done += take;
    }
    return done;
  }

 private:
  struct Block {
    unsigned long offset = 0, length = 0;
  } blocks[BLOCK_COUNT];
  unsigned char* bytes = nullptr;
  size_t next = 0;
};
