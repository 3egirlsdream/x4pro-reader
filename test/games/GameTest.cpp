#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "apps/games/Game2048.h"
#include "apps/games/Gomoku.h"
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
static uint32_t rng = 1309;
uint32_t next() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
uint64_t sum(const Game2048& g) {
  uint64_t n = 0;
  for (auto v : g.cells)
    if (v) n += uint64_t(1) << v;
  return n;
}
void test2048() {
  Game2048 a, b;
  a.reset(123);
  b.reset(123);
  CHECK(!std::memcmp(&a, &b, sizeof(a)));
  CHECK(sum(a) >= 4 && sum(a) <= 8);
  uint8_t bytes[Game2048::SAVE_SIZE];
  CHECK(!b.load(nullptr, Game2048::SAVE_SIZE));
  a.save(bytes);
  CHECK(b.load(bytes, sizeof(bytes)));
  CHECK(!std::memcmp(&a, &b, sizeof(a)));
  bytes[6] ^= 1;
  CHECK(!b.load(bytes, sizeof(bytes)));
  a.save(bytes);
  CHECK(!b.load(bytes, sizeof(bytes) - 1));
  bytes[4] = 32;
  put32(bytes + 28, checksum(bytes, 28));
  CHECK(!b.load(bytes, sizeof(bytes)));
  a.save(bytes);
  put32(bytes + 24, 0);
  put32(bytes + 28, checksum(bytes, 28));
  CHECK(!b.load(bytes, sizeof(bytes)));
  std::memset(a.cells, 0, 16);
  a.cells[0] = 1;
  a.cells[1] = 1;
  a.cells[2] = 1;
  a.cells[3] = 1;
  a.score = 0;
  CHECK(a.move(Direction::Left));
  CHECK(a.cells[0] == 2 && a.cells[1] == 2);
  CHECK(a.score == 8);
  std::memset(a.cells, 0, 16);
  a.cells[0] = 1;
  a.cells[1] = 1;
  a.cells[2] = 2;
  a.score = 0;
  CHECK(a.move(Direction::Left));
  CHECK(a.cells[0] == 2 && a.cells[1] == 2 && a.score == 4);
  std::memset(a.cells, 0, 16);
  a.cells[0] = 10;
  a.cells[1] = 10;
  a.score = 0;
  CHECK(a.move(Direction::Left));
  CHECK(a.won() && a.score == 2048);
  CHECK(!a.over());
  a.cells[1] = 11;
  CHECK(a.move(Direction::Left));
  CHECK(a.cells[0] == 12);
  for (int i = 0; i < 16; ++i) a.cells[i] = 1 + (i + i / 4) % 2;
  CHECK(a.over());
  a.save(bytes);
  const auto random = a.random;
  const auto score = a.score;
  CHECK(!a.move(Direction::Left));
  CHECK(a.random == random && a.score == score);
  CHECK(!a.spawn());
  a.cells[1] = a.cells[0];
  CHECK(!a.over());
  // Independent tile-by-tile reference: slide one tile at a time, marking merged destinations.
  for (int trial = 0; trial < 12000; ++trial) {
    for (auto& cell : a.cells) cell = next() % 6;
    a.score = 0;
    a.random = next() | 1;
    const auto direction = static_cast<Direction>(trial % 4);
    std::array<uint8_t, 16> expected;
    std::copy(std::begin(a.cells), std::end(a.cells), expected.begin());
    bool merged[16]{};
    uint32_t expectedScore = 0;
    int delta = direction == Direction::Left    ? -1
                : direction == Direction::Right ? 1
                : direction == Direction::Up    ? -4
                                                : 4;
    for (int k = 0; k < 16; ++k) {
      const int i = delta > 0 ? 15 - k : k;
      if (!expected[i]) continue;
      int at = i;
      while (true) {
        const int to = at + delta;
        if (to < 0 || to >= 16 || (std::abs(delta) == 1 && to / 4 != at / 4)) break;
        if (!expected[to]) {
          expected[to] = expected[at];
          expected[at] = 0;
          at = to;
          continue;
        }
        if (expected[to] == expected[at] && !merged[to]) {
          ++expected[to];
          expectedScore += uint32_t(1) << expected[to];
          merged[to] = true;
          expected[at] = 0;
        }
        break;
      }
    }
    const bool changed = std::memcmp(a.cells, expected.data(), 16) != 0;
    const auto before = sum(a);
    const auto randomBefore = a.random;
    CHECK(a.move(direction) == changed);
    CHECK(a.score == expectedScore);
    int spawned = 0;
    for (int i = 0; i < 16; ++i) {
      if (expected[i])
        CHECK(a.cells[i] == expected[i]);
      else if (a.cells[i]) {
        ++spawned;
        CHECK(a.cells[i] == 1 || a.cells[i] == 2);
      }
    }
    CHECK(spawned == (changed ? 1 : 0));
    CHECK(changed ? sum(a) == before + 2 || sum(a) == before + 4 : a.random == randomBefore);
  }
  // Round trips preserve the random stream, so resuming yields identical future tiles.
  a.reset(491);
  b = a;
  for (int i = 0; i < 300; ++i) {
    auto d = static_cast<Direction>(next() % 4);
    CHECK(a.move(d) == b.move(d));
    a.save(bytes);
    CHECK(b.load(bytes, sizeof(bytes)));
    CHECK(!std::memcmp(&a, &b, sizeof(a)));
  }
  // Defined exponent/score saturation avoids overflow in very long continued games.
  std::memset(a.cells, 0, 16);
  a.cells[0] = a.cells[1] = 30;
  a.score = UINT32_MAX - 1;
  CHECK(a.move(Direction::Left));
  CHECK(a.cells[0] == 31 && a.score == UINT32_MAX);
}
void testGomoku() {
  Gomoku g;
  for (int d = 0; d < 4; ++d) {
    const int delta[] = {1, 15, 16, -14};
    for (int color = 1; color <= 2; ++color) {
      g.reset();
      const int start = d == 3 ? 9 * 15 + 2 : 2 * 15 + 2;
      for (int i = 0; i < 4; ++i) g.cells[start + i * delta[d]] = color;
      CHECK(!g.wins(start + 3 * delta[d], color));
      g.cells[start + 4 * delta[d]] = color;
      CHECK(g.wins(start, color));
      g.cells[start + 5 * delta[d]] = color;
      CHECK(g.wins(start + 5 * delta[d], color));
    }
  }
  g.reset();
  CHECK(!g.play(-1) && !g.play(225));
  CHECK(g.play(112));
  CHECK(!g.play(112));
  CHECK(g.turn == 2);
  uint8_t bytes[Gomoku::SAVE_SIZE];
  g.save(bytes);
  Gomoku restored;
  CHECK(!restored.load(nullptr, Gomoku::SAVE_SIZE));
  CHECK(restored.load(bytes, sizeof(bytes)));
  CHECK(!std::memcmp(g.cells, restored.cells, 225));
  bytes[20] ^= 1;
  CHECK(!restored.load(bytes, sizeof(bytes)));
  g.save(bytes);
  bytes[2] = 1;
  put32(bytes + 236, checksum(bytes, 236));
  CHECK(!restored.load(bytes, sizeof(bytes)));
  g.reset(2);
  g.save(bytes);
  CHECK(restored.load(bytes, sizeof(bytes)) && restored.human == 2);
  g.reset();
  for (int i = 0; i < 5; ++i) {
    CHECK(g.play(i));
    if (i < 4) CHECK(g.play(30 + i));
  }
  CHECK(g.result == 1);
  CHECK(!g.play(190));
  g.save(bytes);
  CHECK(restored.load(bytes, sizeof(bytes)) && restored.result == 1);
  // A full legal board without any five in all four directions.
  g.reset();
  for (int turn = 0; turn < 225; ++turn) {
    int chosen = -1;
    for (int i = 0; i < 225; ++i)
      if (!g.cells[i] && uint8_t(((i % 15 + 2 * (i / 15)) % 4 < 2) ? 1 : 2) == g.turn) {
        chosen = i;
        break;
      }
    CHECK(chosen >= 0 && g.play(chosen));
    if (turn < 224) CHECK(g.result == 0);
  }
  CHECK(g.result == 3);
  g.save(bytes);
  CHECK(restored.load(bytes, sizeof(bytes)) && restored.result == 3);
  bytes[3] = 0;
  put32(bytes + 236, checksum(bytes, 236));
  CHECK(!restored.load(bytes, sizeof(bytes)));
  // Row wrap must never count as a horizontal line.
  g.reset();
  for (int i = 13; i < 18; ++i) g.cells[i] = 1;
  CHECK(!g.wins(15, 1));
}
int search(Gomoku& g) {
  GomokuAI ai;
  ai.begin(g);
  unsigned calls = 0;
  while (ai.running()) {
    const auto nodes = ai.nodes();
    ai.step(7);
    CHECK(ai.nodes() - nodes <= 7);
    CHECK(++calls < 600);
  }
  CHECK(ai.nodes() <= GomokuAI::MAX_NODES);
  return ai.move();
}
void testAI() {
  Gomoku g;
  g.reset(2);
  CHECK(search(g) == 112);
  const int deltas[] = {1, 15, 16, -14};
  for (int d = 0; d < 4; ++d)
    for (int color = 1; color <= 2; ++color) {
      const int start = d == 3 ? 9 * 15 + 2 : 2 * 15 + 2, delta = deltas[d];
      g.reset();
      g.turn = color;
      for (int i = 0; i < 4; ++i) g.cells[start + i * delta] = color;
      g.cells[start - delta] = 3 - color;
      CHECK(search(g) == start + 4 * delta);
      g.reset();
      g.turn = color;
      for (int i = 0; i < 4; ++i) g.cells[start + i * delta] = 3 - color;
      g.cells[start - delta] = color;
      CHECK(search(g) == start + 4 * delta);
    }
  // Own immediate win takes priority over a competing opponent four.
  g.reset();
  g.turn = 1;
  for (int i = 0; i < 4; ++i) {
    g.cells[30 + i] = 1;
    g.cells[75 + i] = 2;
  }
  const int win = search(g);
  CHECK(g.wins(win, 1));
  g.reset();
  g.play(112);
  g.play(113);
  g.play(97);
  g.play(127);
  GomokuAI ai;
  const Gomoku before = g;
  for (unsigned amount : {0u, 1u, 224u, 225u, 250u, 1500u}) {
    ai.begin(g);
    ai.step(amount);
    ai.cancel();
    const auto nodes = ai.nodes();
    const auto move = ai.move();
    CHECK(ai.step(4096));
    CHECK(ai.nodes() == nodes && ai.move() == move && !ai.running());
    CHECK(!std::memcmp(g.cells, before.cells, 225));
  }
  // A deadline before the tactical pass completes must still return a legal fallback on late boards.
  Gomoku dense;
  dense.reset();
  for (int i = 0; i < 80; ++i) dense.cells[i] = 1 + (i % 2);
  dense.cells[112] = 1;
  ai.begin(dense);
  ai.step(1);
  ai.cancel();
  CHECK(ai.move() >= 0 && ai.move() < 225 && !dense.cells[ai.move()]);
  const int answer = search(g);
  CHECK(answer >= 0 && answer < 225 && !g.cells[answer]);
  CHECK(search(g) == answer);
  // Play complete deterministic games to exercise late boards and terminal/cancel paths.
  for (int round = 0; round < 4; ++round) {
    g.reset();
    for (int ply = 0; ply < 225 && !g.result; ++ply) {
      const int move = search(g);
      CHECK(g.play(move));
    }
    CHECK(g.result != 0);
  }
}
void benchmark() {
  Gomoku g;
  g.reset();
  for (int i : {112, 113, 97, 127, 96, 128, 111, 98, 126, 110}) g.play(i);
  GomokuAI ai;
  using Clock = std::chrono::steady_clock;
  unsigned maxNodes = 0;
  double worstUs = 0, worstSlice = 0, totalUs = 0;
  for (int n = 0; n < 200; ++n) {
    const auto start = Clock::now();
    ai.begin(g);
    while (ai.running()) {
      const auto slice = Clock::now();
      ai.step(1);
      worstSlice = std::max(worstSlice, std::chrono::duration<double, std::micro>(Clock::now() - slice).count());
    }
    const auto us = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    totalUs += us;
    worstUs = std::max(worstUs, us);
    maxNodes = std::max(maxNodes, ai.nodes());
  }
  std::printf(
      "BENCH host_only searches=200 mean_us=%.2f max_us=%.2f max_step_us=%.2f max_nodes=%u sizeof_2048=%zu "
      "sizeof_gomoku=%zu sizeof_ai=%zu\n",
      totalUs / 200, worstUs, worstSlice, maxNodes, sizeof(Game2048), sizeof(Gomoku), sizeof(GomokuAI));
}
int main() {
  test2048();
  testGomoku();
  testAI();
  benchmark();
  std::printf("PASS %u checks\n", checks);
}
