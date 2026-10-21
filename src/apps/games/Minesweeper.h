#pragma once
#include <cstddef>
#include <cstdint>
namespace games {
class Minesweeper {
 public:
  static constexpr int MAX_CELLS = 225;
  static constexpr size_t SAVE_SIZE = 240;
  static constexpr uint8_t MINE = 1, OPEN = 2, FLAG = 4;
  enum Result : uint8_t { Running, Won, Lost };
  uint8_t cells[MAX_CELLS]{};
  uint8_t level = 0;
  Result result = Running;
  bool generated = false;
  uint32_t random = 1;
  int16_t exploded = -1;
  void reset(uint32_t seed, uint8_t difficulty = 0);
  int width() const;
  int count() const { return width() * width(); }
  int mineCount() const;
  int flags() const;
  int opened() const;
  int adjacent(int index) const;
  bool reveal(int index);
  bool toggleFlag(int index);
  bool chord(int index);
  void save(uint8_t* out) const;
  bool load(const uint8_t* data, size_t size);

 private:
  // Reused for mine placement and iterative flood fill; no recursion or hot-path heap.
  uint8_t queue[MAX_CELLS]{};
  uint32_t nextRandom();
  void generate(int safeIndex);
  bool openRegion(int index);
  void checkWin();
};
}  // namespace games
