#pragma once
#include <cstddef>
#include <cstdint>
namespace games {
class Gomoku {
 public:
  static constexpr int SIZE = 15;
  static constexpr int COUNT = SIZE * SIZE;
  static constexpr size_t SAVE_SIZE = 240;
  uint8_t cells[COUNT]{};
  uint8_t turn = 1;
  uint8_t result = 0;  // 0 running, 1 black, 2 white, 3 draw.
  uint8_t human = 1;
  int16_t last = -1;
  void reset(uint8_t humanColor = 1);
  bool play(int index);
  bool wins(int index, uint8_t color) const;
  bool near(int index) const;
  int value(int index, uint8_t color) const;
  void save(uint8_t* out) const;
  bool load(const uint8_t* data, size_t size);
};
class GomokuAI {
 public:
  static constexpr unsigned MAX_NODES = 4096;
  static constexpr unsigned CANDIDATES = 12;
  void begin(const Gomoku& game);
  void cancel();
  bool step(unsigned operations = 8);
  bool running() const { return active; }
  int move() const { return winning >= 0 ? winning : blocking >= 0 ? blocking : best; }
  unsigned nodes() const { return visited; }

 private:
  Gomoku board;
  uint8_t nearMask[(Gomoku::COUNT + 7) / 8]{};
  struct Candidate {
    int index = -1;
    int score = 0;
  } candidates[CANDIDATES];
  uint8_t color = 1;
  bool active = false;
  unsigned visited = 0;
  int phase = 0, scan = 0, count = 0, root = 0, reply = 0;
  int best = -1, bestScore = -2000000000, worstReply = 0;
  int winning = -1, blocking = -1;
  void insert(int index, int score);
};
}  // namespace games
