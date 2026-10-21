#include "GameSave.h"

#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "apps/AppStorage.h"
namespace games {
namespace save {
constexpr char HEX_DIGITS[] = "0123456789abcdef";
int unhex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
bool decode(JsonVariantConst doc, uint8_t* out, size_t count) {
  if (!doc["schema"].is<int>() || doc["schema"].as<int>() != 1 || !doc["board"].is<const char*>()) return false;
  const char* text = doc["board"].as<const char*>();
  if (strlen(text) != count * 2) return false;
  for (size_t i = 0; i < count; ++i) {
    const int a = unhex(text[2 * i]), b = unhex(text[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = a * 16 + b;
  }
  return true;
}
bool valid2048(JsonVariantConst doc) {
  uint8_t bytes[Game2048::SAVE_SIZE];
  Game2048 game;
  return decode(doc, bytes, sizeof(bytes)) && game.load(bytes, sizeof(bytes));
}
bool validGomoku(JsonVariantConst doc) {
  // Recovery validation is cold-path only; keep its two boards off the small activity task stack.
  struct Scratch {
    uint8_t bytes[Gomoku::SAVE_SIZE];
    Gomoku game;
  };
  auto scratch = makeUniqueNoThrow<Scratch>();
  if (!scratch) {
    LOG_ERR("GAME", "Cannot allocate save validation");
    return false;
  }
  return decode(doc, scratch->bytes, sizeof(scratch->bytes)) &&
         scratch->game.load(scratch->bytes, sizeof(scratch->bytes));
}
bool validMinesweeper(JsonVariantConst doc) {
  // Cold-path validation keeps the fixed flood queue and serialized board off the task stack.
  struct Scratch {
    uint8_t bytes[Minesweeper::SAVE_SIZE];
    Minesweeper game;
  };
  auto scratch = makeUniqueNoThrow<Scratch>();
  if (!scratch) {
    LOG_ERR("GAME", "Cannot allocate mines save validation");
    return false;
  }
  return decode(doc, scratch->bytes, sizeof(scratch->bytes)) &&
         scratch->game.load(scratch->bytes, sizeof(scratch->bytes));
}
bool write(const char* id, const uint8_t* bytes, size_t count, char* text) {
  for (size_t i = 0; i < count; ++i) {
    text[i * 2] = HEX_DIGITS[bytes[i] >> 4];
    text[i * 2 + 1] = HEX_DIGITS[bytes[i] & 15];
  }
  text[count * 2] = 0;
  JsonDocument doc;
  doc["schema"] = 1;
  doc["board"] = text;
  return appStorage::save(id, "state", doc);
}
}  // namespace save
}  // namespace games
