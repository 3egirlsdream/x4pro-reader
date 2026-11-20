#pragma once

#include <HalStorage.h>
#include <Logging.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace ProgressFile {

enum class Format { Epub, Txt, Xtc };

inline bool open(const std::string& cachePath, Format format, HalFile& file) {
  for (const char* suffix : {"/progress.bin", "/progress.bin.tmp", "/progress.bin.bak"}) {
    if (!Storage.openFileForRead("PRG", cachePath + suffix, file)) continue;
    const size_t size = file.size();
    if (size == 4 || (format == Format::Epub && (size == 6 || size == 10)) || (format == Format::Txt && size == 12))
      return true;
    file.close();
  }
  return false;
}

inline bool writeAtomic(const std::string& cachePath, const uint8_t* data, size_t len) {
  const auto final = cachePath + "/progress.bin";
  const auto ready = final + ".tmp";
  const auto backup = final + ".bak";
  const auto part = final + ".part";
  HalFile file;
  if (!Storage.openFileForWrite("PRG", part, file) || file.write(data, len) != len || !file.close()) {
    LOG_ERR("PRG", "Failed to stage progress");
    return false;
  }
  // Only fully closed records reach ready. Recover the interrupted rotation before reusing its slots.
  if (Storage.exists(ready.c_str())) {
    if (Storage.exists(final.c_str())) {
      if (!Storage.remove(ready.c_str())) return false;
    } else if (!Storage.rename(ready.c_str(), final.c_str()))
      return false;
  }
  if (!Storage.rename(part.c_str(), ready.c_str())) return false;
  if (Storage.exists(final.c_str())) {
    if (Storage.exists(backup.c_str()) && !Storage.remove(backup.c_str())) return false;
    if (!Storage.rename(final.c_str(), backup.c_str())) return false;
  }
  if (!Storage.rename(ready.c_str(), final.c_str())) {
    LOG_ERR("PRG", "Recovering staged progress on next open");
    return false;
  }
  return true;
}
}  // namespace ProgressFile
