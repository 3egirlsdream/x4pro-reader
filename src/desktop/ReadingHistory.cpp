#include "ReadingHistory.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <LibraryFormat.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace desktop {
namespace {
struct DiskStamp {
  uint32_t magic = 0x44534831;
  uint32_t check = 0;
  uint64_t sequence = 0;
  uint64_t pathHash = 0;
  int16_t percent = -1;
  uint8_t finished = 0;
  uint8_t reserved[5]{};
};
static_assert(sizeof(DiskStamp) == 32);
uint32_t checksum(DiskStamp value) {
  value.check = 0;
  uint32_t result = 2166136261u;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(value); ++i) result = (result ^ bytes[i]) * 16777619u;
  return result;
}
std::string stampPath(uint64_t hash) {
  char name[64];
  snprintf(name, sizeof(name), "/.crosspoint/desktop/%016llx.bin", static_cast<unsigned long long>(hash));
  return name;
}
bool load(const std::string& path, DiskStamp& value) {
  HalFile file;
  DiskStamp candidate;
  if (!Storage.openFileForRead("DESKTOP", path, file) || file.size() != sizeof(candidate) ||
      file.read(&candidate, sizeof(candidate)) != sizeof(candidate) || candidate.magic != 0x44534831 ||
      candidate.check != checksum(candidate) || candidate.percent < -1 || candidate.percent > 100 ||
      candidate.finished > 1)
    return false;
  value = candidate;
  return true;
}
bool save(const std::string& path, DiskStamp& value) {
  value.check = checksum(value);
  const auto tmp = path + ".tmp";
  const auto backup = path + ".bak";
  {
    HalFile file;
    if (!Storage.openFileForWrite("DESKTOP", tmp, file) ||
        file.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value)) != sizeof(value))
      return false;
    file.flush();
  }
  Storage.remove(backup.c_str());
  const bool exists = Storage.exists(path.c_str());
  if (exists && !Storage.rename(path.c_str(), backup.c_str())) return false;
  if (!Storage.rename(tmp.c_str(), path.c_str())) {
    if (exists) Storage.rename(backup.c_str(), path.c_str());
    return false;
  }
  Storage.remove(backup.c_str());
  return true;
}
bool loadRecover(const std::string& path, DiskStamp& value) { return load(path, value) || load(path + ".bak", value); }
}  // namespace
std::string cachePathFor(const std::string& path) {
  const char* kind = FsHelpers::hasEpubExtension(path) ? "epub" : FsHelpers::hasXtcExtension(path) ? "xtc" : "txt";
  return std::string("/.crosspoint/") + kind + "_" + std::to_string(std::hash<std::string>{}(path));
}
ReadingStamp readStamp(const std::string& path) {
  const uint64_t hash = library::clixPathHash(path.data(), path.size());
  DiskStamp value;
  if (!loadRecover(stampPath(hash), value) || value.pathHash != hash) return {};
  return {value.sequence, value.percent, bool(value.finished)};
}
void rememberRead(const std::string& path, int percent, bool finished) {
  DiskStamp head;
  loadRecover("/.crosspoint/desktop/sequence.bin", head);
  DiskStamp value;
  const auto previous = readStamp(path);
  value.sequence = std::max(head.sequence, previous.sequence) + 1;
  value.pathHash = library::clixPathHash(path.data(), path.size());
  value.percent = std::clamp(percent, -1, finished ? 100 : 99);
  value.finished = finished;
  Storage.mkdir("/.crosspoint/desktop");
  Storage.remove("/.crosspoint/desktop-order.bin");
  // Reserve the sequence before publishing a book; a failed reservation cannot reorder old reads.
  head.sequence = value.sequence;
  if (!save("/.crosspoint/desktop/sequence.bin", head) || !save(stampPath(value.pathHash), value))
    LOG_ERR("DESKTOP", "Reading history save failed");
}
}  // namespace desktop
