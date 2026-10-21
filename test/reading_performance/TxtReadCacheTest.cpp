#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#include "TxtReadCache.h"
struct Input {
  std::vector<uint8_t> data;
  size_t calls = 0;
  bool fail = false;
};
bool read(void* ctx, uint8_t* out, size_t at, size_t count) {
  auto& in = *static_cast<Input*>(ctx);
  ++in.calls;
  if (in.fail) {
    if (count) out[0] = 0;
    return false;
  }
  assert(at <= in.data.size() && count <= in.data.size() - at);
  memcpy(out, in.data.data() + at, count);
  return true;
}
int main() {
  Input input;
  input.data.resize(100000);
  for (size_t i = 0; i < input.data.size(); ++i) input.data[i] = i * 37;
  std::array<uint8_t, TxtReadCache::CAPACITY> memory;
  std::array<uint8_t, 8192> out;
  TxtReadCache cache;
  cache.reset(memory.data());
  for (size_t offset = 0; offset < input.data.size(); offset += 127) {
    auto count = std::min(out.size(), input.data.size() - offset);
    assert(cache.read(read, &input, input.data.size(), out.data(), offset, count));
    assert(!memcmp(out.data(), input.data.data() + offset, count));
  }
  assert(input.calls < 20);
  for (size_t offset = 80000; offset > 0; offset -= 1000) {
    assert(cache.read(read, &input, input.data.size(), out.data(), offset, out.size()));
    assert(!memcmp(out.data(), input.data.data() + offset, out.size()));
  }
  cache.reset(memory.data());
  input.fail = true;
  assert(!cache.read(read, &input, input.data.size(), out.data(), 90000, out.size()));
  input.fail = false;
  assert(cache.read(read, &input, input.data.size(), out.data(), 90000, out.size()));
  assert(!memcmp(out.data(), input.data.data() + 90000, out.size()));
  assert(!cache.read(read, &input, input.data.size(), out.data(), std::numeric_limits<size_t>::max(), 1));
  assert(!cache.read(read, &input, input.data.size(), out.data(), 99999, 2));
  cache.reset(nullptr);
  const auto before = input.calls;
  assert(cache.read(read, &input, input.data.size(), out.data(), 0, out.size()));
  assert(input.calls == before + 1);
  assert(!memcmp(out.data(), input.data.data(), out.size()));
  std::cout << "TXT read-ahead: sequential/random/backward/EOF/overflow/failed-fill/retry/no-PSRAM passed\n";
}
