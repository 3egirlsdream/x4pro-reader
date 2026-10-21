#include <Arduino.h>
#include <HalDisplay.h>
#include <HalStorage.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "apps/AppStorage.h"
#include "apps/NativeApp.h"
#include "apps/games/GameSave.h"
#include "apps/games/MinesweeperActivity.h"
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
void tap(Activity& app, MappedInputManager& i, int x, int y) {
  i.tapped = true;
  i.tx = x;
  i.ty = y;
  frame(app, i);
}
void holdCell(Activity& app, MappedInputManager& i, int x, int y) {
  i.longTouch = true;
  i.tx = x;
  i.ty = y;
  frame(app, i);
}
void key(Activity& app, MappedInputManager& i, Button b) {
  i.released = int(b);
  frame(app, i);
}
void snapshot(GfxRenderer& r, const std::string& folder, const char* name) {
  std::ofstream out(folder + "/" + name + ".json");
  serializeJson(r.document, out);
}
void store(const Minesweeper& g) {
  uint8_t bytes[240];
  char text[481];
  g.save(bytes);
  assert(save::write("game-mines", bytes, sizeof(bytes), text));
}
Minesweeper read() {
  JsonDocument doc;
  uint8_t bytes[240];
  Minesweeper g;
  assert(appStorage::load("game-mines", "state", doc, save::validMinesweeper));
  assert(save::decode(doc.as<JsonVariantConst>(), bytes, sizeof(bytes)));
  assert(g.load(bytes, sizeof(bytes)));
  return g;
}
int main(int argc, char** argv) {
  assert(argc == 3);
  previewChinese = std::string(argv[1]) == "zh";
  const std::string folder = argv[2];
  std::filesystem::create_directories(folder);
  GfxRenderer renderer;
  MappedInputManager input;
  renderer.setOrientation(GfxRenderer::LandscapeClockwise);
  {
    MinesweeperActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    assert(renderer.getOrientation() == GfxRenderer::Portrait);
    snapshot(renderer, folder, "mines-new");
    const auto frames = display.frames;
    for (int i = 0; i < 2000; ++i) frame(app, input);
    assert(display.frames == frames);
    tap(app, input, 5, 150);
    assert(display.frames == frames);
    tap(app, input, 43, 177);
    assert(display.frames == frames + 1);  // select, no reveal
    holdCell(app, input, 43, 177);
    snapshot(renderer, folder, "mines-flag");
    const auto flaggedFrames = display.frames;
    key(app, input, Button::Confirm);
    assert(display.frames == flaggedFrames);
    app.onExit();
    auto g = read();
    assert(!g.generated && g.flags() == 1);
    assert(renderer.getOrientation() == GfxRenderer::LandscapeClockwise);
  }
  {
    MinesweeperActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    holdCell(app, input, 43, 177);
    tap(app, input, 43, 177);
    snapshot(renderer, folder, "mines-open");
    // Mode button, long-Up shortcut, physical navigation and activation.
    tap(app, input, 240, 622);
    input.longUp = true;
    frame(app, input);
    key(app, input, Button::Down);
    key(app, input, Button::Up);
    const auto frames = display.frames;
    key(app, input, Button::Confirm);
    assert(display.frames == frames);  // revealed zero does not chord
    input.longDown = true;
    frame(app, input);
    snapshot(renderer, folder, "mines-menu");
    tap(app, input, 240, 300);
    snapshot(renderer, folder, "mines-medium");
    input.longDown = true;
    frame(app, input);
    tap(app, input, 240, 365);
    snapshot(renderer, folder, "mines-hard");
    key(app, input, Button::Confirm);
    app.onExit();
    auto g = read();
    assert(g.level == 2 && g.generated && g.result != Minesweeper::Lost);
  }
  {
    const auto calls = testStorage.renameCalls;
    MinesweeperActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    app.onExit();
    assert(testStorage.renameCalls == calls);  // unchanged reopen does not write to SD
  }
  {
    auto g = read();
    int mine = -1;
    for (int i = 0; i < g.count(); ++i)
      if (g.cells[i] & Minesweeper::MINE) {
        mine = i;
        break;
      }
    assert(g.reveal(mine) && g.result == Minesweeper::Lost);
    store(g);
    MinesweeperActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    snapshot(renderer, folder, "mines-lost");
    const auto frames = display.frames;
    key(app, input, Button::Confirm);
    assert(display.frames == frames);
    key(app, input, Button::Back);
    assert(openedApps == 1);
    app.onExit();
  }
  {
    Minesweeper g;
    g.reset(11, 1);
    g.reveal(72);
    for (int i = 0; i < g.count(); ++i)
      if (!(g.cells[i] & (Minesweeper::MINE | Minesweeper::OPEN))) g.reveal(i);
    assert(g.result == Minesweeper::Won);
    store(g);
    MinesweeperActivity app(renderer, input);
    app.onEnter();
    frame(app, input);
    snapshot(renderer, folder, "mines-won");
    assert(app.handleForcedRefresh());
    frame(app, input);
    app.requestUpdate();
    frame(app, input);  // repaint after a global overlay closes
    tap(app, input, 60, 755);
    assert(openedApps == 2);
    app.onExit();
  }
  std::cout << "PASS mines production Activity: touch select/reveal/hold flag, mode switch, keys, 3 difficulties, "
               "save/resume, terminal states, orientation, menu, overlays, unchanged-save and 2000 idle frames; sizeof="
            << sizeof(MinesweeperActivity) << "\n";
}
