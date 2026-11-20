#include "GameActivity.h"

#include <Arduino.h>
#include <HalDisplay.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "GameSave.h"
#include "apps/AppStorage.h"
#include "apps/NativeApp.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace games {
using Button = MappedInputManager::Button;
namespace {
void disc(const GfxRenderer& renderer, int cx, int cy, int radius, bool black) {
  for (int y = -radius; y <= radius; ++y) {
    int x = radius;
    while (x * x + y * y > radius * radius) --x;
    renderer.drawLine(cx - x, cy + y, cx + x, cy + y, black);
  }
}
}  // namespace
void GameActivity::update(bool board) {
  ++revision;
  if (board) {
    changed = true;
    ++boardUpdates;
  }
  requestUpdate();
}
void GameActivity::layout() {
  safe = UITheme::getInstance().getScreenSafeArea(renderer, false, false);
  const auto& m = UITheme::getInstance().getMetrics();
  top = safe.y + m.topPadding + m.headerHeight;
  footer = safe.y + safe.height - 68;
  const int extent = std::min(safe.width - 36, footer - top - 90);
  unit = extent / boardDivisions();
  boardX = safe.x + (safe.width - unit * boardDivisions()) / 2;
  boardY = top + 64;
}
void GameActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  previous = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);
  load();
  layout();
  resumeWork();
  LOG_INF("GAME", "%s activity bytes: %u", id, unsigned(activityBytes()));
  LOG_INF("GAME", "Heap enter free=%u max=%u", unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
  requestUpdate();
}
void GameActivity::onExit() {
  stopWork();
  if (changed) save();
  LOG_INF("GAME", "Heap exit free=%u max=%u", unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
  renderer.setOrientation(previous);
  Activity::onExit();
}
void GameActivity::center(int y, const char* text, int font) const {
  UITheme::drawCenteredText(renderer, safe, font, y, text);
}
void GameActivity::loop() {
  if (mappedInput.wasReleased(Button::Back)) {
    openApps(renderer, mappedInput);
    return;
  }
  GameInput input;
  for (const auto button : {Button::Up, Button::Down, Button::Left, Button::Right, Button::Confirm})
    if (mappedInput.wasReleased(button)) input.buttons |= 1u << static_cast<unsigned>(button);
  input.longDown = mappedInput.wasLongPressed(Button::Down, 700);
  input.tap = mappedInput.wasScreenTapped(input.x, input.y);
  if (!menu && usesFlagInput()) {
    input.longUp = mappedInput.wasLongPressed(Button::Up, 700);
    int x = 0, y = 0;
    if (mappedInput.wasScreenLongPress(x, y)) {
      input.hold = true;
      input.x = x;
      input.y = y;
    }
  }
  input.swipe = mappedInput.wasSwipe();
  if (input.active()) events.push(input);
  if (events.empty() && !skipLoopDelay()) return;
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  const GameInput event = events.empty() ? GameInput{} : events.pop();
  const int x = event.x, y = event.y;
  const bool tapped = event.tap;
  const bool footerTap = tapped && y >= footer && y < footer + 60 && x >= safe.x && x < safe.x + safe.width;
  if (footerTap && x < safe.x + safe.width / 3) {
    openApps(renderer, mappedInput);
    return;
  }
  if (footerTap || event.longDown) {
    menu = !menu;
    menuIndex = 0;
    if (menu)
      stopWork();
    else
      resumeWork();
    update();
    return;
  }
  if (menu) {
    const int count = menuCount();
    if (event.released(Button::Up)) {
      menuIndex = (menuIndex + count - 1) % count;
      update();
    }
    if (event.released(Button::Down)) {
      menuIndex = (menuIndex + 1) % count;
      update();
    }
    bool activate = event.released(Button::Confirm);
    if (tapped && y >= boardY && y < boardY + count * 64 && x >= safe.x && x < safe.x + safe.width) {
      menuIndex = (y - boardY) / 64;
      activate = true;
    }
    if (activate) {
      activateMenu(menuIndex);
      menu = false;
      resumeWork();
      update();
    }
    return;
  }
  inputGame(event);
}
const char* GameActivity::menuLabel(int index) const {
  if (!index) return tr(STR_GAME_RESUME);
  if (gomoku()) return index == 1 ? tr(STR_GAME_FIRST) : tr(STR_GAME_SECOND);
  return tr(STR_GAME_NEW);
}
void GameActivity::activateMenu(int index) {
  if (index) newGame(index == 2);
}
bool GameActivity::handleForcedRefresh() {
  RenderLock lock;
  forceFull = true;
  update();
  return true;
}
void GameActivity::render(RenderLock&&) {
  // The manager also requests repaint when an overlay closes or charging status changes.
  // Only state changes request game paints; an external request must restore the whole scene.
  const bool first = drawn == 0;
  drawn = revision;
  display.setInverted(false);
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{safe.x, safe.y + m.topPadding, safe.width, m.headerHeight}, I18N.get(title), nullptr,
                 false);
  if (menu) {
    const int count = menuCount();
    for (int i = 0; i < count; ++i) {
      const char* label = menuLabel(i);
      if (i == menuIndex) renderer.drawRect(safe.x + 12, boardY + i * 64 - 4, safe.width - 24, 60, 2, true);
      center(boardY + i * 64 + 12, label, UI_12_FONT_ID);
    }
  } else
    drawGame();
  renderer.drawLine(safe.x, footer - 8, safe.x + safe.width, footer - 8);
  UITheme::drawCenteredText(renderer, Rect{safe.x, footer, safe.width / 3, 50}, UI_10_FONT_ID, footer + 10,
                            tr(STR_BACK));
  UITheme::drawCenteredText(renderer, Rect{safe.x + safe.width / 3, footer, safe.width * 2 / 3, 50}, UI_10_FONT_ID,
                            footer + 10, tr(STR_GAME_MENU));
  const bool full = first || forceFull || boardUpdates >= 12;
  forceFull = false;
  if (full) boardUpdates = 0;
  renderer.displayBuffer(full ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
}
void Game2048Activity::load() {
  JsonDocument doc;
  if (appStorage::load(id, "state", doc, save::valid2048) &&
      save::decode(doc.as<JsonVariantConst>(), binary, Game2048::SAVE_SIZE) && game.load(binary, Game2048::SAVE_SIZE))
    return;
  newGame(false);
}
void Game2048Activity::save() {
  game.save(binary);
  if (!save::write(id, binary, Game2048::SAVE_SIZE, encoded)) LOG_ERR("GAME", "2048 save failed");
}
void Game2048Activity::newGame(bool) {
  game.reset(micros());
  changed = true;
  ++boardUpdates;
}
void Game2048Activity::inputGame(const GameInput& event) {
  auto swipe = event.swipe;
  Direction direction = Direction::Left;
  bool move = true;
  if (swipe == MappedInputManager::SwipeDir::Left || event.released(Button::Left))
    direction = Direction::Left;
  else if (swipe == MappedInputManager::SwipeDir::Right || event.released(Button::Right))
    direction = Direction::Right;
  else if (swipe == MappedInputManager::SwipeDir::Up)
    direction = Direction::Up;
  else if (swipe == MappedInputManager::SwipeDir::Down)
    direction = Direction::Down;
  else if (event.released(Button::Up))
    direction = horizontal ? Direction::Left : Direction::Up;
  else if (event.released(Button::Down))
    direction = horizontal ? Direction::Right : Direction::Down;
  else
    move = false;
  if (event.released(Button::Confirm)) {
    horizontal = !horizontal;
    update();
  }
  if (move && game.move(direction)) update(true);
}
void Game2048Activity::drawGame() {
  char text[80];
  snprintf(text, sizeof(text), tr(STR_GAME_SCORE), static_cast<unsigned long>(game.score));
  center(top + 4, text, UI_12_FONT_ID);
  center(top + 32,
         game.over()  ? tr(STR_GAME_OVER)
         : game.won() ? tr(STR_GAME_2048_WON)
                      : tr(STR_GAME_SWIPE),
         UI_10_FONT_ID);
  for (int i = 0; i < 16; ++i) {
    const int x = boardX + (i % 4) * unit, y = boardY + (i / 4) * unit;
    renderer.drawRect(x + 2, y + 2, unit - 4, unit - 4, game.cells[i] ? 2 : 1, true);
    if (game.cells[i]) {
      snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(uint32_t(1) << game.cells[i]));
      const int font = renderer.getTextWidth(UI_12_FONT_ID, text) < unit - 8 ? UI_12_FONT_ID : SMALL_FONT_ID;
      UITheme::drawCenteredText(renderer, Rect{x, y, unit, unit}, font, y + unit / 2 - 12, text);
    }
  }
  center(boardY + unit * 4 + 12, horizontal ? tr(STR_GAME_AXIS_H) : tr(STR_GAME_AXIS_V), SMALL_FONT_ID);
}
void GomokuActivity::load() {
  JsonDocument doc;
  if (appStorage::load(id, "state", doc, save::validGomoku) &&
      save::decode(doc.as<JsonVariantConst>(), binary, Gomoku::SAVE_SIZE) && game.load(binary, Gomoku::SAVE_SIZE)) {
    cursor = game.last >= 0 ? game.last : 112;
    return;
  }
  newGame(false);
}
void GomokuActivity::save() {
  game.save(binary);
  if (!save::write(id, binary, Gomoku::SAVE_SIZE, encoded)) LOG_ERR("GAME", "Gomoku save failed");
}
void GomokuActivity::newGame(bool alternate) {
  ai.cancel();
  game.reset(alternate ? 2 : 1);
  cursor = 112;
  changed = true;
  ++boardUpdates;
}
void GomokuActivity::resumeWork() {
  if (!game.result && game.turn != game.human) {
    ai.begin(game);
    computeUs = 0;
    searchStartedMs = 0;
  }
}
void GomokuActivity::place() {
  if (game.turn != game.human || !game.play(cursor)) return;
  update(true);
  resumeWork();
}
void GomokuActivity::inputGame(const GameInput& event) {
  if (ai.running()) {
    if (!computeUs) searchStartedMs = millis();
    const uint32_t start = micros();
    // A slice checks time after every bounded local evaluation; no tasks or recursive search.
    for (unsigned i = 0; i < SLICE_NODES && ai.running(); ++i) {
      ai.step(1);
      if (uint32_t(micros() - start) >= SLICE_US) break;
    }
    computeUs += uint32_t(micros() - start);
    if (computeUs >= COMPUTE_LIMIT_US || uint32_t(millis() - searchStartedMs) >= WALL_LIMIT_MS) ai.cancel();
    if (!ai.running()) {
      const int index = ai.move();
      LOG_INF("GAME", "AI nodes=%u compute_us=%lu", ai.nodes(), static_cast<unsigned long>(computeUs));
      if (game.play(index)) {
        cursor = index;
        update(true);
      }
    }
    return;
  }
  const int x = event.x, y = event.y;
  if (!game.result && game.turn == game.human && event.tap) {
    const int left = boardX - unit / 2, upper = boardY - unit / 2;
    if (x >= left && y >= upper && x < boardX + 14 * unit + unit / 2 && y < boardY + 14 * unit + unit / 2) {
      const int col = (x - left) / unit, row = (y - upper) / unit;
      const int next = row * 15 + col;
      if (next == cursor)
        place();
      else {
        cursor = next;
        update();
      }
    }
  }
  int next = cursor;
  if (event.released(Button::Up)) next = (cursor + 224) % 225;
  if (event.released(Button::Down)) next = (cursor + 1) % 225;
  if (event.released(Button::Left)) next = cursor % 15 ? cursor - 1 : cursor;
  if (event.released(Button::Right)) next = cursor % 15 < 14 ? cursor + 1 : cursor;
  if (next != cursor) {
    cursor = next;
    update();
  }
  if (event.released(Button::Confirm)) place();
}
void GomokuActivity::drawGame() {
  const char* status = game.result == 3 ? tr(STR_GAME_DRAW)
                       : game.result    ? (game.result == game.human ? tr(STR_GAME_YOU_WIN) : tr(STR_GAME_AI_WIN))
                       : game.turn == game.human ? tr(STR_GAME_YOUR_TURN)
                                                 : tr(STR_GAME_THINKING);
  center(top + 4, status, UI_12_FONT_ID);
  center(top + 32, game.human == 1 ? tr(STR_GAME_BLACK) : tr(STR_GAME_WHITE), UI_10_FONT_ID);
  for (int i = 0; i < 15; ++i) {
    renderer.drawLine(boardX, boardY + i * unit, boardX + 14 * unit, boardY + i * unit);
    renderer.drawLine(boardX + i * unit, boardY, boardX + i * unit, boardY + 14 * unit);
  }
  for (const int index : {48, 56, 112, 168, 176})
    renderer.fillRect(boardX + index % 15 * unit - 2, boardY + index / 15 * unit - 2, 5, 5);
  for (int i = 0; i < 225; ++i) {
    if (!game.cells[i]) continue;
    const int x = boardX + i % 15 * unit, y = boardY + i / 15 * unit, radius = std::max(3, unit / 2 - 3);
    disc(renderer, x, y, radius, true);
    if (game.cells[i] == 2) disc(renderer, x, y, radius - 2, false);
    if (i == game.last) renderer.fillRect(x - 2, y - 2, 5, 5, game.cells[i] != 1);
  }
  if (!game.result) {
    const int x = boardX + cursor % 15 * unit, y = boardY + cursor / 15 * unit;
    renderer.drawRect(x - unit / 2, y - unit / 2, unit, unit, 2, true);
  }
  center(boardY + unit * 14 + 18, tr(STR_GAME_SELECT_HINT), SMALL_FONT_ID);
}
}  // namespace games
