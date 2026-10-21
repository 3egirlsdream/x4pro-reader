#include <HalStorage.h>

#include <cassert>
#include <cstring>
#include <iostream>

#include "apps/AppStorage.h"
#include "apps/games/GameSave.h"
FakeStorage testStorage;
using namespace games;
int main() {
  Minesweeper g, restored;
  uint8_t bytes[Minesweeper::SAVE_SIZE];
  char text[Minesweeper::SAVE_SIZE * 2 + 1];
  JsonDocument doc;
  g.reset(42, 2);
  g.toggleFlag(0);
  g.save(bytes);
  assert(save::write("game-mines", bytes, sizeof(bytes), text));
  g.reveal(112);
  g.save(bytes);
  assert(save::write("game-mines", bytes, sizeof(bytes), text));
  assert(appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  assert(save::decode(doc.as<JsonVariantConst>(), bytes, sizeof(bytes)) && restored.load(bytes, sizeof(bytes)));
  assert(restored.generated && restored.width() == 15 && restored.flags() == 1);
  // Well-formed JSON with a broken payload must recover the previous unopened board.
  deserializeJson(doc, testStorage.files["/.crosspoint/apps/game-mines/state.json"]);
  std::string bad = doc["board"].as<const char*>();
  bad[22] = bad[22] == '0' ? '1' : '0';
  doc["board"] = bad;
  auto& primary = testStorage.files["/.crosspoint/apps/game-mines/state.json"];
  primary.clear();
  serializeJson(doc, primary);
  assert(appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  assert(save::decode(doc.as<JsonVariantConst>(), bytes, sizeof(bytes)) && restored.load(bytes, sizeof(bytes)));
  assert(!restored.generated && restored.flags() == 1);
  // Restore, then simulate interruption before the temp file is installed.
  g.save(bytes);
  assert(save::write("game-mines", bytes, sizeof(bytes), text));
  testStorage.renameCalls = 0;
  testStorage.failRename = 2;
  assert(!save::write("game-mines", bytes, sizeof(bytes), text));
  testStorage.failRename = 0;
  assert(appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  Game2048 tiles;
  tiles.reset(71);
  tiles.save(bytes);
  assert(save::write("game-2048", bytes, Game2048::SAVE_SIZE, text));
  Gomoku stones;
  stones.reset();
  stones.play(112);
  stones.save(bytes);
  assert(save::write("game-gomoku", bytes, Gomoku::SAVE_SIZE, text));
  assert(appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  assert(appStorage::load("game-2048", "state", doc, save::valid2048));
  assert(appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  testStorage.files["/.crosspoint/apps/game-mines/state.json.bak"] = "broken";
  testStorage.files.erase("/.crosspoint/apps/game-mines/state.json.tmp");
  assert(!appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  std::cout
      << "PASS mines production JSON/SD codec, corrupt backup recovery, interrupted rotation and three namespaces\n";
}
