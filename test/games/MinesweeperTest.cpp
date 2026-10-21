#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "apps/games/Game2048.h"
#include "apps/games/Minesweeper.h"
using namespace games;
static unsigned checks = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    ++checks;                                                           \
    if (!(x)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
      std::exit(1);                                                     \
    }                                                                   \
  } while (0)
int referenceAdjacent(const Minesweeper& g, int index) {
  int count = 0;
  for (int i = 0; i < g.count(); ++i)
    if (i != index && std::abs(i % g.width() - index % g.width()) <= 1 &&
        std::abs(i / g.width() - index / g.width()) <= 1 && (g.cells[i] & Minesweeper::MINE))
      ++count;
  return count;
}
void referenceFlood(const Minesweeper& g, int first, bool* expected) {
  expected[first] = true;
  // Independent repeated fixed-point expansion, without the production queue algorithm.
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < g.count(); ++i) {
      if (!expected[i] || referenceAdjacent(g, i)) continue;
      for (int n = 0; n < g.count(); ++n) {
        if (std::abs(n % g.width() - i % g.width()) > 1 || std::abs(n / g.width() - i / g.width()) > 1 || expected[n] ||
            (g.cells[n] & (Minesweeper::FLAG | Minesweeper::MINE)))
          continue;
        expected[n] = true;
        changed = true;
      }
    }
  }
}
void generatedBoards() {
  Minesweeper g, other;
  uint8_t bytes[Minesweeper::SAVE_SIZE], resumed[Minesweeper::SAVE_SIZE];
  for (int level = 0; level < 3; ++level) {
    g.reset(1, level);
    const int count = g.count();
    for (uint32_t seed = 1; seed <= 16; ++seed)
      for (int first = 0; first < count; ++first) {
        g.reset(seed, level);
        CHECK(!g.generated && !g.opened() && !g.flags());
        CHECK(g.reveal(first));
        CHECK(g.generated && g.result != Minesweeper::Lost);
        CHECK(g.adjacent(first) == 0);
        int mines = 0;
        for (int i = 0; i < g.count(); ++i) {
          mines += (g.cells[i] & Minesweeper::MINE) != 0;
          if (std::abs(i % g.width() - first % g.width()) <= 1 && std::abs(i / g.width() - first / g.width()) <= 1)
            CHECK(!(g.cells[i] & Minesweeper::MINE));
          CHECK(!(g.cells[i] & Minesweeper::OPEN) || !(g.cells[i] & Minesweeper::MINE));
        }
        CHECK(mines == g.mineCount());
        g.save(bytes);
        CHECK(other.load(bytes, sizeof(bytes)));
        other.save(resumed);
        CHECK(!std::memcmp(bytes, resumed, sizeof(bytes)));
        CHECK(!g.reveal(first));
      }
    // Compare complete zero-region expansion with a structurally independent reference.
    for (int seed = 1; seed <= 8; ++seed) {
      g.reset(seed, level);
      CHECK(g.toggleFlag(0));
      CHECK(g.reveal(count / 2));
      bool expected[Minesweeper::MAX_CELLS]{};
      referenceFlood(g, count / 2, expected);
      for (int i = 0; i < count; ++i) {
        CHECK(bool(g.cells[i] & Minesweeper::OPEN) == expected[i]);
        CHECK(g.adjacent(i) == referenceAdjacent(g, i));
      }
      CHECK(g.cells[0] & Minesweeper::FLAG);
    }
  }
}
void rulesAndSave() {
  Minesweeper g, restored;
  uint8_t bytes[Minesweeper::SAVE_SIZE], before[Minesweeper::SAVE_SIZE];
  g.reset(0, 99);
  CHECK(g.width() == 9 && g.random == 1);
  CHECK(!g.reveal(-1) && !g.reveal(81) && !g.toggleFlag(-1) && !g.toggleFlag(81));
  CHECK(!g.chord(0) && !g.chord(225));
  CHECK(g.toggleFlag(40));
  CHECK(g.flags() == 1);
  CHECK(!g.reveal(40) && !g.generated);
  g.save(bytes);
  CHECK(restored.load(bytes, sizeof(bytes)));
  CHECK(restored.flags() == 1 && !restored.generated);
  CHECK(g.toggleFlag(40));
  CHECK(g.reveal(40));
  CHECK(restored.toggleFlag(40));
  CHECK(restored.reveal(40));
  g.save(bytes);
  restored.save(before);
  CHECK(!std::memcmp(bytes, before, sizeof(bytes)));
  CHECK(!g.toggleFlag(40));
  int mine = -1;
  for (int i = 0; i < g.count(); ++i)
    if (g.cells[i] & Minesweeper::MINE) {
      mine = i;
      break;
    }
  CHECK(mine >= 0);
  CHECK(g.reveal(mine));
  CHECK(g.result == Minesweeper::Lost && g.exploded == mine);
  g.save(bytes);
  CHECK(restored.load(bytes, sizeof(bytes)));
  CHECK(restored.result == Minesweeper::Lost);
  CHECK(!g.reveal(0) && !g.toggleFlag(0) && !g.chord(40));
  for (int level = 0; level < 3; ++level) {
    g.reset(921, level);
    CHECK(g.reveal(g.count() / 2));
    for (int i = 0; i < g.count(); ++i)
      if (!(g.cells[i] & (Minesweeper::MINE | Minesweeper::OPEN))) CHECK(g.reveal(i));
    CHECK(g.result == Minesweeper::Won && g.opened() == g.count() - g.mineCount());
    g.save(bytes);
    CHECK(restored.load(bytes, sizeof(bytes)));
    CHECK(restored.result == Minesweeper::Won);
  }
  g.reset(91, 2);
  CHECK(g.reveal(112));
  g.save(bytes);
  restored = g;
  for (int i = 0; i < int(sizeof(bytes)); ++i)
    for (int bit = 0; bit < 8; ++bit) {
      bytes[i] ^= 1u << bit;
      CHECK(!restored.load(bytes, sizeof(bytes)));
      bytes[i] ^= 1u << bit;
    }
  CHECK(!restored.load(nullptr, sizeof(bytes)));
  CHECK(!restored.load(bytes, sizeof(bytes) - 1));
  auto reject = [&](int offset, uint8_t value) {
    g.save(bytes);
    bytes[offset] = value;
    put32(bytes + 236, checksum(bytes, 236));
    restored.save(before);
    CHECK(!restored.load(bytes, sizeof(bytes)));
    uint8_t after[240];
    restored.save(after);
    CHECK(!std::memcmp(before, after, sizeof(before)));
  };
  reject(0, 2);
  reject(1, 3);
  reject(2, 0);
  reject(3, 3);
  reject(3, Minesweeper::Won);
  reject(3, Minesweeper::Lost);
  reject(8, 225);
  reject(9, 1);
  reject(10, 1);
  reject(11 + 112, Minesweeper::OPEN | Minesweeper::FLAG);
  reject(11 + 112, 8);
  g.reset(22);
  g.save(bytes);
  bytes[11 + 100] = Minesweeper::FLAG;
  put32(bytes + 236, checksum(bytes, 236));
  CHECK(!restored.load(bytes, sizeof(bytes)));
}
void chord() {
  auto fixture = []() {
    Minesweeper g;
    g.reset(89);
    g.generated = true;
    for (int i : {30, 31, 32, 0, 8, 72, 80, 10, 20, 60}) g.cells[i] = Minesweeper::MINE;
    g.cells[40] = Minesweeper::OPEN;
    return g;
  };
  auto g = fixture();
  CHECK(g.adjacent(40) == 3);
  CHECK(!g.chord(40));
  for (int i : {30, 31, 32}) CHECK(g.toggleFlag(i));
  CHECK(g.chord(40));
  CHECK(g.result != Minesweeper::Lost);
  CHECK(g.opened() > 1);
  CHECK(!g.chord(40));
  g = fixture();
  for (int i : {30, 31, 39}) CHECK(g.toggleFlag(i));
  CHECK(g.chord(40));
  CHECK(g.result == Minesweeper::Lost && g.exploded == 32);
  uint8_t bytes[240];
  g.save(bytes);
  Minesweeper restored;
  CHECK(restored.load(bytes, sizeof(bytes)));
}
void benchmark() {
  Minesweeper g;
  using Clock = std::chrono::steady_clock;
  double total = 0, worst = 0;
  for (int i = 1; i <= 10000; ++i) {
    const auto start = Clock::now();
    g.reset(i, 2);
    g.reveal(112);
    const auto us = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    total += us;
    worst = std::max(worst, us);
  }
  std::printf(
      "BENCH host_only mines_first_open trials=10000 mean_us=%.2f max_us=%.2f sizeof_model=%zu queue_capacity=225\n",
      total / 10000, worst, sizeof(Minesweeper));
}
int main() {
  generatedBoards();
  rulesAndSave();
  chord();
  benchmark();
  std::printf("PASS mines %u checks\n", checks);
}
