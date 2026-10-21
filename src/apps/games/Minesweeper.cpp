#include "Minesweeper.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Game2048.h"
namespace games {
namespace {
constexpr uint8_t WIDTHS[] = {9, 12, 15};
constexpr uint8_t MINES[] = {10, 24, 45};
int neighbor(int index, int dx, int dy, int width) {
  const int x = index % width + dx, y = index / width + dy;
  return x >= 0 && x < width && y >= 0 && y < width ? y * width + x : -1;
}
}  // namespace
void Minesweeper::reset(uint32_t seed, uint8_t difficulty) {
  memset(cells, 0, sizeof(cells));
  level = difficulty < 3 ? difficulty : 0;
  result = Running;
  generated = false;
  random = seed ? seed : 1;
  exploded = -1;
}
int Minesweeper::width() const { return WIDTHS[level < 3 ? level : 0]; }
int Minesweeper::mineCount() const { return MINES[level < 3 ? level : 0]; }
int Minesweeper::flags() const {
  int total = 0;
  for (int i = 0; i < count(); ++i) total += (cells[i] & FLAG) != 0;
  return total;
}
int Minesweeper::opened() const {
  int total = 0;
  for (int i = 0; i < count(); ++i) total += (cells[i] & OPEN) != 0;
  return total;
}
uint32_t Minesweeper::nextRandom() {
  random ^= random << 13;
  random ^= random >> 17;
  random ^= random << 5;
  return random;
}
int Minesweeper::adjacent(int index) const {
  if (index < 0 || index >= count()) return 0;
  int total = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      if (!dx && !dy) continue;
      const int n = neighbor(index, dx, dy, width());
      if (n >= 0) total += (cells[n] & MINE) != 0;
    }
  return total;
}
void Minesweeper::generate(int safeIndex) {
  int available = 0;
  const int w = width();
  for (int i = 0; i < count(); ++i)
    if (std::abs(i % w - safeIndex % w) > 1 || std::abs(i / w - safeIndex / w) > 1) queue[available++] = i;
  // Partial Fisher-Yates has a fixed number of iterations, even for dense boards.
  for (int i = 0; i < mineCount(); ++i) {
    const int chosen = i + nextRandom() % (available - i);
    std::swap(queue[i], queue[chosen]);
    cells[queue[i]] |= MINE;
  }
  generated = true;
}
bool Minesweeper::toggleFlag(int index) {
  if (result != Running || index < 0 || index >= count() || (cells[index] & OPEN)) return false;
  cells[index] ^= FLAG;
  return true;
}
bool Minesweeper::openRegion(int index) {
  if (cells[index] & (OPEN | FLAG)) return false;
  cells[index] |= OPEN;
  if (cells[index] & MINE) {
    result = Lost;
    exploded = index;
    return true;
  }
  int head = 0, tail = 0;
  if (!adjacent(index)) queue[tail++] = index;
  while (head < tail) {
    const int current = queue[head++];
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        const int n = neighbor(current, dx, dy, width());
        if (n < 0 || (cells[n] & (MINE | OPEN | FLAG))) continue;
        cells[n] |= OPEN;  // Mark before enqueue, so each cell is visited at most once.
        if (!adjacent(n)) queue[tail++] = n;
      }
  }
  return true;
}
void Minesweeper::checkWin() {
  if (result == Running && opened() == count() - mineCount()) result = Won;
}
bool Minesweeper::reveal(int index) {
  if (result != Running || index < 0 || index >= count() || (cells[index] & (OPEN | FLAG))) return false;
  if (!generated) generate(index);
  const bool changed = openRegion(index);
  checkWin();
  return changed;
}
bool Minesweeper::chord(int index) {
  if (result != Running || index < 0 || index >= count() || !(cells[index] & OPEN) || !adjacent(index)) return false;
  int marked = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      const int n = neighbor(index, dx, dy, width());
      if (n >= 0) marked += (cells[n] & FLAG) != 0;
    }
  if (marked != adjacent(index)) return false;
  bool changed = false;
  for (int dy = -1; dy <= 1 && result == Running; ++dy)
    for (int dx = -1; dx <= 1 && result == Running; ++dx) {
      const int n = neighbor(index, dx, dy, width());
      if (n >= 0) changed |= openRegion(n);
    }
  checkWin();
  return changed;
}
void Minesweeper::save(uint8_t* out) const {
  memset(out, 0, SAVE_SIZE);
  out[0] = 1;
  out[1] = level;
  out[2] = generated;
  out[3] = result;
  put32(out + 4, random);
  out[8] = exploded < 0 ? 255 : exploded;
  memcpy(out + 11, cells, MAX_CELLS);
  put32(out + 236, checksum(out, 236));
}
bool Minesweeper::load(const uint8_t* data, size_t size) {
  if (!data || size != SAVE_SIZE || data[0] != 1 || data[1] > 2 || data[2] > 1 || data[3] > Lost || !get32(data + 4) ||
      data[9] || data[10] || get32(data + 236) != checksum(data, 236))
    return false;
  const int total = WIDTHS[data[1]] * WIDTHS[data[1]], mines = MINES[data[1]];
  int actualMines = 0, safeOpen = 0, mineOpen = 0;
  for (int i = 0; i < MAX_CELLS; ++i) {
    const uint8_t cell = data[i + 11];
    if (cell > 7 || ((cell & OPEN) && (cell & FLAG)) || (i >= total && cell)) return false;
    actualMines += (cell & MINE) != 0;
    safeOpen += (cell & OPEN) && !(cell & MINE);
    mineOpen += (cell & OPEN) && (cell & MINE);
  }
  const int hit = data[8] == 255 ? -1 : data[8];
  if (!data[2]) {
    if (actualMines || safeOpen || mineOpen || hit != -1 || data[3] != Running) return false;
  } else {
    if (actualMines != mines || !safeOpen) return false;
    if (data[3] == Lost) {
      if (hit < 0 || hit >= total || (data[hit + 11] & (MINE | OPEN)) != (MINE | OPEN) || mineOpen != 1) return false;
    } else if (hit != -1 || mineOpen)
      return false;
    if ((data[3] == Won) != (safeOpen == total - mines)) return false;
  }
  level = data[1];
  generated = data[2];
  result = static_cast<Result>(data[3]);
  random = get32(data + 4);
  exploded = hit;
  memcpy(cells, data + 11, MAX_CELLS);
  return true;
}
}  // namespace games
