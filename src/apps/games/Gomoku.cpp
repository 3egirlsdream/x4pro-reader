#include "Gomoku.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Game2048.h"
namespace games {
namespace {
constexpr int DX[] = {1, 0, 1, 1};
constexpr int DY[] = {0, 1, 1, -1};
bool inside(int x, int y) { return x >= 0 && x < 15 && y >= 0 && y < 15; }
}  // namespace
void Gomoku::reset(uint8_t humanColor) {
  memset(cells, 0, sizeof(cells));
  turn = 1;
  human = humanColor == 2 ? 2 : 1;
  result = 0;
  last = -1;
}
bool Gomoku::wins(int index, uint8_t color) const {
  if (index < 0 || index >= COUNT || color < 1 || color > 2) return false;
  for (int d = 0; d < 4; ++d) {
    int total = 1;
    for (int sign : {-1, 1}) {
      int x = index % SIZE + sign * DX[d], y = index / SIZE + sign * DY[d];
      while (inside(x, y) && cells[y * SIZE + x] == color) {
        ++total;
        if (total >= 5) return true;
        x += sign * DX[d];
        y += sign * DY[d];
      }
    }
  }
  return false;
}
bool Gomoku::play(int index) {
  if (result || index < 0 || index >= COUNT || cells[index]) return false;
  cells[index] = turn;
  last = index;
  if (wins(index, turn))
    result = turn;
  else {
    result = 3;
    for (auto cell : cells)
      if (!cell) {
        result = 0;
        break;
      }
  }
  turn = 3 - turn;
  return true;
}
bool Gomoku::near(int index) const {
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) {
      const int x = index % SIZE + dx, y = index / SIZE + dy;
      if (inside(x, y) && cells[y * SIZE + x]) return true;
    }
  return false;
}
int Gomoku::value(int index, uint8_t color) const {
  if (wins(index, color)) return 1000000;
  // Score only the four local lines, including broken threes/fours in five-cell windows.
  constexpr int WEIGHTS[] = {0, 2, 16, 160, 4000, 1000000};
  int score = 0;
  for (int d = 0; d < 4; ++d) {
    for (int start = -4; start <= 0; ++start) {
      int stones = 0;
      bool valid = true;
      for (int i = 0; i < 5; ++i) {
        const int x = index % SIZE + (start + i) * DX[d], y = index / SIZE + (start + i) * DY[d];
        if (!inside(x, y)) {
          valid = false;
          break;
        }
        const auto cell = y * SIZE + x == index ? color : cells[y * SIZE + x];
        if (cell && cell != color) {
          valid = false;
          break;
        }
        stones += cell == color;
      }
      if (valid) score += WEIGHTS[stones];
    }
  }
  return score;
}
void Gomoku::save(uint8_t* out) const {
  memset(out, 0, SAVE_SIZE);
  out[0] = 1;
  out[1] = human;
  out[2] = turn;
  out[3] = result;
  memcpy(out + 4, cells, COUNT);
  out[229] = last < 0 ? 255 : last;
  put32(out + 236, checksum(out, 236));
}
bool Gomoku::load(const uint8_t* data, size_t size) {
  if (!data || size != SAVE_SIZE || data[0] != 1 || data[1] < 1 || data[1] > 2 || data[2] < 1 || data[2] > 2 ||
      data[3] > 3 || get32(data + 236) != checksum(data, 236))
    return false;
  int black = 0, white = 0;
  for (int i = 0; i < COUNT; ++i) {
    if (data[i + 4] > 2) return false;
    black += data[i + 4] == 1;
    white += data[i + 4] == 2;
  }
  if (black < white || black > white + 1 || data[2] != (black == white ? 1 : 2)) return false;
  const int lastIndex = data[229] == 255 ? -1 : data[229];
  if ((black + white == 0) != (lastIndex == -1) || lastIndex >= COUNT ||
      (lastIndex >= 0 && data[lastIndex + 4] != 3 - data[2]))
    return false;
  // Validate before touching this object, so a bad save cannot partially replace a game.
  // Reuse the serialized board through a small accessor to avoid a second board on the stack.
  bool foundWin = false;
  for (int i = 0; i < COUNT; ++i) {
    if (!data[i + 4]) continue;
    for (int d = 0; d < 4; ++d) {
      int length = 0;
      bool throughLast = false;
      for (int n = 0; n < 5; ++n) {
        const int x = i % SIZE + n * DX[d], y = i / SIZE + n * DY[d];
        if (!inside(x, y) || data[y * SIZE + x + 4] != data[i + 4]) break;
        throughLast |= y * SIZE + x == lastIndex;
        ++length;
      }
      if (length == 5) {
        if (data[3] != data[i + 4] || data[3] != 3 - data[2] || !throughLast) return false;
        foundWin = true;
      }
    }
  }
  if (data[3] == 3 && black + white != COUNT) return false;
  if ((data[3] == 1 || data[3] == 2) && !foundWin) return false;
  if (!data[3] && black + white == COUNT) return false;
  memcpy(cells, data + 4, COUNT);
  turn = data[2];
  human = data[1];
  last = lastIndex;
  result = data[3];
  return true;
}
void GomokuAI::insert(int index, int score) {
  int slot = std::min(count, int(CANDIDATES) - 1);
  if (count == int(CANDIDATES) && score <= candidates[slot].score) return;
  while (slot > 0 && candidates[slot - 1].score < score) {
    if (slot < int(CANDIDATES)) candidates[slot] = candidates[slot - 1];
    --slot;
  }
  candidates[slot] = {index, score};
  if (count < int(CANDIDATES)) ++count;
}
void GomokuAI::begin(const Gomoku& game) {
  board = game;
  memset(nearMask, 0, sizeof(nearMask));
  color = board.turn;
  active = !board.result;
  visited = 0;
  phase = scan = count = root = reply = 0;
  best = winning = blocking = -1;
  if (active) {
    if (!board.cells[112])
      best = 112;
    else
      for (int i = 0; i < Gomoku::COUNT; ++i)
        if (!board.cells[i]) {
          best = i;
          break;
        }
  }
  bestScore = -2000000000;
  worstReply = 0;
}
void GomokuAI::cancel() { active = false; }
bool GomokuAI::step(unsigned operations) {
  while (active && operations-- && visited < MAX_NODES) {
    ++visited;
    if (phase == 0) {
      const int index = scan++;
      if (!board.cells[index]) {
        if (best < 0) best = index;
        if (board.wins(index, color)) winning = index;
        if (board.wins(index, 3 - color)) blocking = index;
        if (board.near(index)) {
          nearMask[index / 8] |= uint8_t(1u << (index % 8));
          insert(index, board.value(index, color) + board.value(index, 3 - color));
          best = candidates[0].index;
        }
      }
      if (scan == Gomoku::COUNT) {
        if (winning >= 0 || blocking >= 0) {
          best = winning >= 0 ? winning : blocking;
          active = false;
        } else if (!count) {
          best = board.cells[112] ? best : 112;
          active = false;
        } else {
          phase = 1;
          board.cells[candidates[0].index] = color;
        }
      }
    } else {
      // Original proximity is cached in 29 bytes; a root move adds only its own 5x5 neighborhood.
      const int rootIndex = candidates[root].index;
      const bool nearRoot = std::abs(reply % 15 - rootIndex % 15) <= 2 && std::abs(reply / 15 - rootIndex / 15) <= 2;
      if (!board.cells[reply] && ((nearMask[reply / 8] & (1u << (reply % 8))) || nearRoot))
        worstReply = std::max(worstReply, board.value(reply, 3 - color));
      if (++reply == Gomoku::COUNT) {
        const int index = candidates[root].index;
        const int score = board.value(index, color) * 2 - worstReply * 3;
        if (score > bestScore) {
          bestScore = score;
          best = index;
        }
        board.cells[index] = 0;
        reply = worstReply = 0;
        if (++root == count)
          active = false;
        else
          board.cells[candidates[root].index] = color;
      }
    }
  }
  if (visited >= MAX_NODES) active = false;
  return !active;
}
}  // namespace games
