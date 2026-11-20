#include "AppStorage.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace appStorage {
bool validId(const char* id) {
  if (!id || !*id || strlen(id) > 40) return false;
  for (const char* p = id; *p; ++p)
    if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '-' && *p != '_') return false;
  return true;
}
namespace {
struct JsonReader {
  HalFile& file;
  int read() { return file.read(); }
  size_t readBytes(char* data, size_t count) {
    const auto read = file.read(data, count);
    return read > 0 ? static_cast<size_t>(read) : 0;
  }
};
bool read(const char* path, JsonDocument& doc, bool (*validate)(JsonVariantConst)) {
  HalFile f;
  if (!Storage.openFileForRead("APP", path, f) || f.size() > 8192) return false;
  JsonReader reader{f};
  const auto result = deserializeJson(doc, reader);
  return result == DeserializationError::Ok && !doc.overflowed() && (!validate || validate(doc.as<JsonVariantConst>()));
}
}  // namespace
bool load(const char* id, const char* file, JsonDocument& doc, bool (*validate)(JsonVariantConst)) {
  if (!validId(id) || !validId(file)) return false;
  char path[160];
  snprintf(path, sizeof(path), "/.crosspoint/apps/%s/%s.json", id, file);
  if (read(path, doc, validate)) return true;
  snprintf(path, sizeof(path), "/.crosspoint/apps/%s/%s.json.bak", id, file);
  doc.clear();
  if (read(path, doc, validate)) return true;
  snprintf(path, sizeof(path), "/.crosspoint/apps/%s/%s.json.tmp", id, file);
  doc.clear();
  return read(path, doc, validate);
}
bool save(const char* id, const char* file, const JsonDocument& doc) {
  if (!validId(id) || !validId(file) || doc.overflowed() || measureJson(doc) > 8192) return false;
  // Cold-path scratch exceeds the task stack budget; one nothrow allocation.
  struct Paths {
    char path[128];
    char temp[136];
    char backup[136];
  };
  auto paths = makeUniqueNoThrow<Paths>();
  if (!paths) {
    LOG_ERR("APP", "Cannot allocate file paths");
    return false;
  }
  auto& path = paths->path;
  auto& temp = paths->temp;
  auto& backup = paths->backup;
  snprintf(path, sizeof(path), "/.crosspoint/apps/%s", id);
  if (!Storage.ensureDirectoryExists(path)) return false;
  snprintf(path, sizeof(path), "/.crosspoint/apps/%s/%s.json", id, file);
  // Only a successfully renamed document becomes visible to load(). Older .tmp files remain readable.
  snprintf(temp, sizeof(temp), "%s.part", path);
  snprintf(backup, sizeof(backup), "%s.bak", path);
  {
    HalFile f;
    if (!Storage.openFileForWrite("APP", temp, f)) return false;
    if (serializeJson(doc, f) != measureJson(doc)) return false;
    if (!f.close()) return false;
  }
  // Preserve a recoverable copy across FAT rename interruption.
  if (Storage.exists(path)) {
    if (Storage.exists(backup) && !Storage.remove(backup)) return false;
    if (!Storage.rename(path, backup)) return false;
  }
  if (!Storage.rename(temp, path)) {
    LOG_ERR("APP", "Cannot install JSON: %s", path);
    return false;
  }
  return true;
}
}  // namespace appStorage
