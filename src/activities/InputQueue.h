#pragma once
#include <array>
#include <cstddef>

// Main-loop only. Keep the earliest input when the display stalls longer than the queue can absorb.
template <class T, size_t Capacity = 8>
class InputQueue {
  std::array<T, Capacity> items{};
  size_t first = 0, count = 0;

 public:
  bool empty() const { return count == 0; }
  void push(T value) {
    if (count == Capacity) return;
    items[(first + count++) % Capacity] = value;
  }
  T pop() {
    const T value = items[first];
    first = (first + 1) % Capacity;
    --count;
    return value;
  }
};
