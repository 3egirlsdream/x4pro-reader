#include <HalStorage.h>

#include <cassert>
#include <cstring>
#include <iostream>

#include "apps/AppStorage.h"
#include "apps/games/GameSave.h"
FakeStorage testStorage;
using namespace games;
int main() {
  Game2048 game, resumed;
  Gomoku g, restored;
  uint8_t binary[Gomoku::SAVE_SIZE];
  char text[Gomoku::SAVE_SIZE * 2 + 1];
  JsonDocument doc;
  game.reset(41);
  game.save(binary);
  assert(save::write("game-2048", binary, Game2048::SAVE_SIZE, text));
  game.move(Direction::Right);
  game.save(binary);
  assert(save::write("game-2048", binary, Game2048::SAVE_SIZE, text));
  assert(appStorage::load("game-2048", "state", doc, save::valid2048));
  assert(save::decode(doc.as<JsonVariantConst>(), binary, Game2048::SAVE_SIZE));
  assert(resumed.load(binary, Game2048::SAVE_SIZE));
  assert(!std::memcmp(&game, &resumed, sizeof(game)));
  g.reset(2);
  g.play(112);
  g.save(binary);
  assert(save::write("game-gomoku", binary, Gomoku::SAVE_SIZE, text));
  assert(appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  assert(save::decode(doc.as<JsonVariantConst>(), binary, Gomoku::SAVE_SIZE));
  assert(restored.load(binary, Gomoku::SAVE_SIZE));
  assert(restored.human == 2 && restored.turn == 2 && restored.cells[112] == 1);
  auto& primary = testStorage.files["/.crosspoint/apps/game-2048/state.json"];
  const auto good = primary;
  for (const auto& corrupt : {std::string("{broken"), std::string("{\"schema\":2}"),
                              std::string("{\"schema\":1,\"board\":\"oops\"}"), std::string(8193, ' ')}) {
    primary = corrupt;
    assert(appStorage::load("game-2048", "state", doc, save::valid2048));
    assert(save::decode(doc.as<JsonVariantConst>(), binary, Game2048::SAVE_SIZE));
    assert(resumed.load(binary, Game2048::SAVE_SIZE));
    Game2048 initial;
    initial.reset(41);
    assert(!std::memcmp(&initial, &resumed, sizeof(initial)));
  }
  primary = good;
  deserializeJson(doc, good);
  std::string bad = doc["board"].as<const char*>();
  bad[12] = bad[12] == '0' ? '1' : '0';
  doc["board"] = bad;
  primary.clear();
  serializeJson(doc, primary);
  assert(appStorage::load("game-2048", "state", doc, save::valid2048));
  // Interrupted rotation recovers a complete previous board, without touching the other namespace.
  primary = good;
  game.save(binary);
  testStorage.renameCalls = 0;
  testStorage.failRename = 2;
  assert(!save::write("game-2048", binary, Game2048::SAVE_SIZE, text));
  testStorage.failRename = 0;
  assert(appStorage::load("game-2048", "state", doc, save::valid2048));
  assert(appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  g.play(113);
  g.save(binary);
  assert(save::write("game-gomoku", binary, Gomoku::SAVE_SIZE, text));
  auto& gp = testStorage.files["/.crosspoint/apps/game-gomoku/state.json"];
  deserializeJson(doc, gp);
  bad = doc["board"].as<const char*>();
  bad[10] = 'z';
  doc["board"] = bad;
  gp.clear();
  serializeJson(doc, gp);
  assert(appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  assert(save::decode(doc.as<JsonVariantConst>(), binary, Gomoku::SAVE_SIZE));
  assert(restored.load(binary, Gomoku::SAVE_SIZE));
  assert(restored.cells[113] == 0);
  testStorage.files.clear();
  assert(!appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  std::cout << "PASS production game JSON codec, checksums, semantic validation, namespace isolation, backup and "
               "interrupted-save recovery\n";
}
