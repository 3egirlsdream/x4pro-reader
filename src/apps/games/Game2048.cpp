#include "Game2048.h"

#include <cstring>
#include <limits>
namespace games {
uint32_t checksum(const uint8_t* data, size_t size) {
  uint32_t value = 2166136261u;
  for (size_t i = 0; i < size; ++i) value = (value ^ data[i]) * 16777619u;
  return value;
}
void put32(uint8_t* out, uint32_t value) {
  for (int i = 0; i < 4; ++i) out[i] = value >> (8 * i);
}
uint32_t get32(const uint8_t* in) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value |= uint32_t(in[i]) << (8 * i);
  return value;
}
void Game2048::reset(uint32_t seed) {
  memset(cells, 0, sizeof(cells));
  score = 0;
  random = seed ? seed : 1;
  spawn();
  spawn();
}
bool Game2048::spawn() {
  uint8_t empty[16];
  unsigned count = 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!cells[i]) empty[count++] = i;
  if (!count) return false;
  auto next = [this]() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return random;
  };
  const auto index = empty[next() % count];
  cells[index] = next() % 10 == 0 ? 2 : 1;
  return true;
}
bool Game2048::move(Direction direction) {
  bool changed = false;
  for (int line = 0; line < 4; ++line) {
    uint8_t values[4]{};
    uint8_t indices[4]{};
    unsigned count = 0;
    for (int i = 0; i < 4; ++i) {
      const int offset = direction == Direction::Right || direction == Direction::Down ? 3 - i : i;
      indices[i] =
          direction == Direction::Left || direction == Direction::Right ? line * 4 + offset : offset * 4 + line;
      if (cells[indices[i]]) values[count++] = cells[indices[i]];
    }
    uint8_t result[4]{};
    unsigned used = 0;
    for (unsigned i = 0; i < count; ++i) {
      auto value = values[i];
      if (i + 1 < count && value == values[i + 1] && value < 31) {
        ++value;
        ++i;
        const uint32_t gain = uint32_t(1) << value;
        score = score > UINT32_MAX - gain ? UINT32_MAX : score + gain;
      }
      result[used++] = value;
    }
    for (int i = 0; i < 4; ++i) {
      changed |= cells[indices[i]] != result[i];
      cells[indices[i]] = result[i];
    }
  }
  if (changed) spawn();
  return changed;
}
bool Game2048::won() const {
  for (auto value : cells)
    if (value >= 11) return true;
  return false;
}
bool Game2048::over() const {
  for (int i = 0; i < 16; ++i) {
    if (!cells[i]) return false;
    if (i % 4 < 3 && cells[i] < 31 && cells[i] == cells[i + 1]) return false;
    if (i < 12 && cells[i] < 31 && cells[i] == cells[i + 4]) return false;
  }
  return true;
}
void Game2048::save(uint8_t* out) const {
  memset(out, 0, SAVE_SIZE);
  out[0] = 1;
  memcpy(out + 4, cells, 16);
  put32(out + 20, score);
  put32(out + 24, random);
  put32(out + 28, checksum(out, 28));
}
bool Game2048::load(const uint8_t* data, size_t size) {
  if (!data || size != SAVE_SIZE || data[0] != 1 || get32(data + 28) != checksum(data, 28) || !get32(data + 24))
    return false;
  unsigned count = 0;
  for (int i = 4; i < 20; ++i) {
    if (data[i] > 31) return false;
    count += data[i] != 0;
  }
  if (count < 2) return false;
  memcpy(cells, data + 4, 16);
  score = get32(data + 20);
  random = get32(data + 24);
  return true;
}
}  // namespace games
