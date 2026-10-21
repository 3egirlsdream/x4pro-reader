#pragma once
#include <array>
#include <cstdint>

// Four equal tiles are invariant under drawIcon's portrait rotation.
constexpr std::array<uint8_t, 128> appGridIcon() {
  std::array<uint8_t, 128> bits{};
  for (auto& byte : bits) byte = 0xff;
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      const int localX = x < 16 ? x - 4 : x - 19;
      const int localY = y < 16 ? y - 4 : y - 19;
      if (localX >= 0 && localX <= 8 && localY >= 0 && localY <= 8 &&
          (localX < 2 || localX > 6 || localY < 2 || localY > 6))
        bits[y * 4 + x / 8] &= ~(1 << (7 - x % 8));
    }
  }
  return bits;
}
inline constexpr auto AppsIcon = appGridIcon();
