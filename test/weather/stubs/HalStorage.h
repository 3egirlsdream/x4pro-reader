#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
struct HalFile {
  std::string* data = nullptr;
  size_t position = 0;
  size_t size() const { return data ? data->size() : 0; }
  int read() { return data && position < data->size() ? static_cast<uint8_t>((*data)[position++]) : -1; }
  int read(void* p, size_t n) {
    const size_t count = data ? std::min(n, data->size() - position) : 0;
    if (count) memcpy(p, data->data() + position, count);
    position += count;
    return count;
  }
  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t* p, size_t n) {
    if (!data) return 0;
    data->append(reinterpret_cast<const char*>(p), n);
    return n;
  }
  void flush() {}
};
struct FakeStorage {
  std::map<std::string, std::string> files;
  int failRename = 0;
  int renameCalls = 0;
  bool ensureDirectoryExists(const char*) { return true; }
  bool exists(const char* p) { return files.count(p) != 0; }
  bool remove(const char* p) { return files.erase(p) != 0; }
  bool rename(const char* a, const char* b) {
    if (++renameCalls == failRename || !exists(a) || exists(b)) return false;
    files[b] = files[a];
    files.erase(a);
    return true;
  }
  bool openFileForRead(const char*, const char* p, HalFile& f) {
    if (!exists(p)) return false;
    f.data = &files[p];
    f.position = 0;
    return true;
  }
  bool openFileForWrite(const char*, const char* p, HalFile& f) {
    files[p].clear();
    f.data = &files[p];
    f.position = 0;
    return true;
  }
};
extern FakeStorage testStorage;
#define Storage testStorage
