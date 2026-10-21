#pragma once
#include <cstddef>
#include <cstdint>
namespace games {
enum class Direction : uint8_t { Left, Right, Up, Down };
class Game2048 {
 public:
  uint8_t cells[16]{};  // Base-two exponents; zero denotes empty.
  uint32_t score = 0;
  uint32_t random = 1;
  void reset(uint32_t seed);
  bool move(Direction direction);
  bool spawn();
  bool won() const;
  bool over() const;
  static constexpr size_t SAVE_SIZE = 32;
  void save(uint8_t* out) const;
  bool load(const uint8_t* data, size_t size);
};
uint32_t checksum(const uint8_t* data, size_t size);
void put32(uint8_t* out, uint32_t value);
uint32_t get32(const uint8_t* in);
}  // namespace games
