#include <Arduino.h>
#include <HalDisplay.h>
#include <HalStorage.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "apps/AppStorage.h"
#include "apps/NativeApp.h"
#include "apps/games/GameActivity.h"
#include "apps/games/GameSave.h"
FakeStorage testStorage;
using namespace games;
using Button = MappedInputManager::Button;
void frame(Activity& app, MappedInputManager& input) {
  app.loop();
  input.clear();
  if (app.requests != app.renderedRequests) {
    app.renderedRequests = app.requests;
    RenderLock lock;
    app.render(std::move(lock));
  }
}
void tap(Activity& app, MappedInputManager& input, int x, int y) {
  input.tapped = true;
  input.tx = x;
  input.ty = y;
  frame(app, input);
}
void key(Activity& app, MappedInputManager& input, Button b) {
  input.released = int(b);
  frame(app, input);
}
void snapshot(GfxRenderer& r, const std::string& folder, const char* name) {
  std::ofstream out(folder + "/" + name + ".json");
  serializeJson(r.document, out);
}
Game2048 read2048() {
  JsonDocument doc;
  uint8_t bytes[32];
  Game2048 g;
  assert(appStorage::load("game-2048", "state", doc, save::valid2048));
  assert(save::decode(doc.as<JsonVariantConst>(), bytes, 32));
  assert(g.load(bytes, 32));
  return g;
}
Gomoku readGomoku() {
  JsonDocument doc;
  uint8_t bytes[240];
  Gomoku g;
  assert(appStorage::load("game-gomoku", "state", doc, save::validGomoku));
  assert(save::decode(doc.as<JsonVariantConst>(), bytes, 240));
  assert(g.load(bytes, 240));
  return g;
}
int main(int argc, char** argv) {
  assert(argc == 3);
  previewChinese = std::string(argv[1]) == "zh";
  std::string folder = argv[2];
  std::filesystem::create_directories(folder);
  GfxRenderer renderer;
  MappedInputManager input;
  renderer.setOrientation(GfxRenderer::LandscapeCounterClockwise);
  {
    Game2048Activity app(renderer, input);
    app.onEnter();
    assert(renderer.getOrientation() == GfxRenderer::Portrait);
    frame(app, input);
    snapshot(renderer, folder, "2048-new");
    const auto frames = display.frames;
    for (int i = 0; i < 1000; ++i) frame(app, input);
    assert(display.frames == frames);
    assert(app.handleForcedRefresh());
    frame(app, input);
    assert(display.frames == frames + 1);
    app.requestUpdate();
    frame(app, input);
    assert(display.frames == frames + 2);
    input.swipe = MappedInputManager::SwipeDir::Left;
    frame(app, input);
    key(app, input, Button::Down);
    key(app, input, Button::Confirm);
    key(app, input, Button::Up);
    snapshot(renderer, folder, "2048-play");
    tap(app, input, 300, 755);
    snapshot(renderer, folder, "2048-menu");
    key(app, input, Button::Down);
    key(app, input, Button::Confirm);
    app.onExit();
    assert(renderer.getOrientation() == GfxRenderer::LandscapeCounterClockwise);
  }
  auto initial = read2048();
  {
    Game2048Activity app(renderer, input);
    app.onEnter();
    frame(app, input);
    app.onExit();
    auto resumed = read2048();
    assert(!std::memcmp(&initial, &resumed, sizeof(initial)));
  }
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    snapshot(renderer, folder, "gomoku-new");
    const auto frames = display.frames;
    for (int i = 0; i < 1000; ++i) frame(app, input);
    assert(display.frames == frames);
    // Select a new intersection: first tap only selects; second places. Off-board taps do nothing.
    tap(app, input, 10, 120);
    assert(display.frames == frames);
    tap(app, input, 54, 184);
    assert(display.frames == frames + 1);
    snapshot(renderer, folder, "gomoku-select");
    tap(app, input, 54, 184);
    snapshot(renderer, folder, "gomoku-thinking");
    unsigned slices = 0;
    while (app.skipLoopDelay()) {
      frame(app, input);
      assert(++slices < 5000);
    }
    for (int i = 0; i < 200; ++i) frame(app, input);
    snapshot(renderer, folder, "gomoku-play");
    app.onExit();
    auto saved = readGomoku();
    assert(saved.cells[16] == 1);
    unsigned count = 0;
    for (auto c : saved.cells) count += c != 0;
    assert(count == 2);
  }
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    tap(app, input, 300, 755);
    snapshot(renderer, folder, "gomoku-menu");
    key(app, input, Button::Down);
    key(app, input, Button::Down);
    key(app, input, Button::Confirm);
    snapshot(renderer, folder, "gomoku-ai-first");
    // Exit during AI's next turn keeps an unmodified board and a resumable turn.
    app.onExit();
    auto saved = readGomoku();
    assert(saved.human == 2 && saved.turn == 1 && saved.last == -1);
  }
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    for (int i = 0; i < 150; ++i) frame(app, input);
    app.onExit();
    auto saved = readGomoku();
    assert(saved.cells[112] == 1 && saved.turn == 2);
  }
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    input.longDown = true;
    frame(app, input);
    key(app, input, Button::Confirm);
    key(app, input, Button::Up);
    key(app, input, Button::Down);
    key(app, input, Button::Confirm);
    // Open menu while thinking cancels immediately; resume restarts from the unchanged game.
    tap(app, input, 300, 755);
    const auto f = display.frames;
    for (int i = 0; i < 100; ++i) frame(app, input);
    assert(display.frames == f);
    key(app, input, Button::Confirm);
    for (int i = 0; i < 200; ++i) frame(app, input);
    key(app, input, Button::Back);
    assert(openedApps == 1);
    tap(app, input, 60, 755);
    assert(openedApps == 2);
    app.onExit();
  }
  // Start from a fresh AI-first game to exercise the clock caps and mid-search menu cancellation.
  auto seedAI = [&]() {
    Gomoku g;
    g.reset(2);
    uint8_t bytes[Gomoku::SAVE_SIZE];
    char text[Gomoku::SAVE_SIZE * 2 + 1];
    g.save(bytes);
    assert(save::write("game-gomoku", bytes, sizeof(bytes), text));
  };
  seedAI();
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    assert(app.skipLoopDelay());
    tap(app, input, 300, 755);
    assert(!app.skipLoopDelay());
    const auto frames = display.frames;
    for (int i = 0; i < 1000; ++i) frame(app, input);
    assert(display.frames == frames);
    key(app, input, Button::Confirm);
    assert(app.skipLoopDelay());
    unsigned slices = 0;
    while (app.skipLoopDelay()) {
      frame(app, input);
      assert(++slices <= 100);
    }
    app.onExit();
    assert(readGomoku().cells[112] == 1);
  }
  seedAI();
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    mockTick = 10000;
    unsigned slices = 0;
    while (app.skipLoopDelay()) {
      frame(app, input);
      assert(++slices <= 12);
    }
    assert(slices > 1);
    mockTick = 1;
    app.onExit();
    assert(readGomoku().turn == 2);
  }
  seedAI();
  {
    GomokuActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    assert(app.skipLoopDelay());
    mockMicros += 3000001;
    frame(app, input);
    assert(!app.skipLoopDelay());
    app.onExit();
    assert(readGomoku().turn == 2);
  }
  std::cout << "PASS production Activity touch, physical keys, AI slices/cancel/resume, menu, exit orientation, "
               "progress persistence clock budgets and 3000 idle frame suppression; sizeof activities="
            << sizeof(Game2048Activity) << "," << sizeof(GomokuActivity) << "\n";
}
