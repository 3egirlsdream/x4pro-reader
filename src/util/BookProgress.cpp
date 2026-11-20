#include "BookProgress.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>

#include "activities/reader/ProgressFile.h"

namespace {
uint32_t readLe32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
}  // namespace

int loadBookProgress(const std::string& path) {
  uint8_t data[10]{};
  if (FsHelpers::hasEpubExtension(path)) {
    // Metadata objects exceed the stack budget; only the featured book is loaded, once per entry.
    auto epub = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: progress metadata");
      return -1;
    }
    if (!epub->load(false, true)) return -1;
    HalFile file;
    if (!ProgressFile::open(epub->getCachePath(), ProgressFile::Format::Epub, file)) return -1;
    const int size = file.read(data, sizeof(data));
    if (size != 4 && size != 6 && size != 10) return -1;
    const int spine = data[0] | (data[1] << 8);
    const int page = data[2] | (data[3] << 8);
    const int total = size >= 6 ? data[4] | (data[5] << 8) : 0;
    if (epub->getSpineItemsCount() <= 0 || epub->getBookSize() == 0) return -1;
    if (spine == epub->getSpineItemsCount()) return 100;
    if (spine > epub->getSpineItemsCount()) return -1;
    const float fraction = total > 0 && page != UINT16_MAX ? std::clamp(float(page) / total, 0.0f, 1.0f) : 0;
    return std::clamp(static_cast<int>(epub->calculateProgress(spine, fraction) * 100 + 0.5f), 0, 99);
  }
  if (FsHelpers::hasXtcExtension(path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: XTC progress metadata");
      return -1;
    }
    if (!xtc->load()) return -1;
    HalFile file;
    if (!ProgressFile::open(xtc->getCachePath(), ProgressFile::Format::Xtc, file) || file.read(data, 4) != 4) return -1;
    const uint32_t page = readLe32(data);
    if (xtc->getPageCount() == 0) return -1;
    if (page >= xtc->getPageCount()) return 100;
    return std::min<int>(99, xtc->calculateProgress(page));
  }
  if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path)) {
    Txt txt(path, "/.crosspoint");
    HalFile file;
    if (!ProgressFile::open(txt.getCachePath(), ProgressFile::Format::Txt, file) || file.read(data, 4) != 4) return -1;
    const uint32_t page = readLe32(data);
    if (file.size() == 12 && file.read(data, 8) == 8) {
      const uint32_t offset = readLe32(data), size = readLe32(data + 4);
      HalFile source;
      if (size && offset < size && Storage.openFileForRead("HOME", path, source) && source.size() == size)
        return std::min<int>(99, offset * 100ULL / size);
      return -1;
    }
    HalFile index;
    // TXT index v3: magic, version, file size, four layout fields, alignment, page count.
    uint8_t header[30];
    if (!Storage.openFileForRead("HOME", txt.getCachePath() + "/index.bin", index) ||
        index.read(header, sizeof(header)) != sizeof(header))
      return -1;
    if (readLe32(header) != 0x54585449 || (header[4] != 3 && header[4] != 4)) return -1;
    const uint32_t flags = readLe32(header + 26);
    const uint32_t pages = header[4] == 3 ? flags : (index.size() - sizeof(header)) / 4;
    const bool complete = header[4] == 3 || (flags & 0x80000000U);
    if (pages == 0 || pages > (index.size() - sizeof(header)) / 4) return -1;
    HalFile source;
    const uint32_t sourceSize = readLe32(header + 5);
    if (!Storage.openFileForRead("HOME", path, source) || source.size() != sourceSize || !sourceSize) return -1;
    if (page >= pages) return complete && page == pages ? 100 : -1;
    // v4 is a prefix index: byte offset is exact even when the final page count is unknown.
    uint8_t offset[4];
    if (!index.seek(sizeof(header) + page * 4) || index.read(offset, 4) != 4) return -1;
    const uint32_t bytes = readLe32(offset);
    if (bytes >= sourceSize) return -1;
    return std::min<int>(99, bytes * 100ULL / sourceSize);
  }
  return -1;
}
