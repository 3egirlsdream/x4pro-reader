#include "MinesweeperActivity.h"

#include <Arduino.h>

#include <cstdio>

#include "GameSave.h"
#include "apps/AppStorage.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace games {
namespace {
using Button = MappedInputManager::Button;
}
const char* MinesweeperActivity::menuLabel(int index) const {
  if (!index) return tr(STR_GAME_RESUME);
  return index == 1 ? tr(STR_GAME_MINES_EASY) : index == 2 ? tr(STR_GAME_MINES_MEDIUM) : tr(STR_GAME_MINES_HARD);
}
void MinesweeperActivity::activateMenu(int index) {
  if (!index) return;
  game.reset(micros(), index - 1);
  cursor = game.count() / 2;
  flagMode = false;
  changed = true;
  ++boardUpdates;
  layout();
}
void MinesweeperActivity::newGame(bool) { activateMenu(1); }
void MinesweeperActivity::load() {
  JsonDocument doc;
  if (appStorage::load(id, "state", doc, save::validMinesweeper) &&
      save::decode(doc.as<JsonVariantConst>(), binary, Minesweeper::SAVE_SIZE) &&
      game.load(binary, Minesweeper::SAVE_SIZE)) {
    cursor = game.count() / 2;
    return;
  }
  newGame(false);
}
void MinesweeperActivity::save() {
  game.save(binary);
  if (!save::write(id, binary, Minesweeper::SAVE_SIZE, encoded)) LOG_ERR("GAME", "Minesweeper save failed");
}
int MinesweeperActivity::hit(int x, int y) const {
  if (x < boardX || y < boardY || x >= boardX + unit * game.width() || y >= boardY + unit * game.width()) return -1;
  return (y - boardY) / unit * game.width() + (x - boardX) / unit;
}
void MinesweeperActivity::act() {
  const bool didChange = flagMode                                   ? game.toggleFlag(cursor)
                         : (game.cells[cursor] & Minesweeper::OPEN) ? game.chord(cursor)
                                                                    : game.reveal(cursor);
  if (didChange) update(true);
}
void MinesweeperActivity::inputGame() {
  if (mappedInput.wasLongPressed(Button::Up, 700)) {
    flagMode = !flagMode;
    update();
    return;
  }
  int x = 0, y = 0;
  if (mappedInput.wasScreenLongPress(x, y)) {
    const int index = hit(x, y);
    if (index >= 0) {
      const bool selected = cursor != index;
      cursor = index;
      if (game.toggleFlag(index))
        update(true);
      else if (selected)
        update();
    }
    return;
  }
  if (mappedInput.wasScreenTapped(x, y)) {
    const int bottom = boardY + unit * game.width();
    if (x >= safe.x + 12 && x < safe.x + safe.width - 12 && y >= bottom + 12 && y < bottom + 56) {
      flagMode = !flagMode;
      update();
      return;
    }
    const int index = hit(x, y);
    if (index >= 0) {
      if (cursor == index)
        act();
      else {
        cursor = index;
        update();
      }
    }
  }
  int next = cursor;
  if (mappedInput.wasReleased(Button::Up)) next = (cursor + game.count() - 1) % game.count();
  if (mappedInput.wasReleased(Button::Down)) next = (cursor + 1) % game.count();
  if (mappedInput.wasReleased(Button::Left) && cursor % game.width()) next = cursor - 1;
  if (mappedInput.wasReleased(Button::Right) && cursor % game.width() < game.width() - 1) next = cursor + 1;
  if (next != cursor) {
    cursor = next;
    update();
  }
  if (mappedInput.wasReleased(Button::Confirm)) act();
}
void MinesweeperActivity::drawGame() {
  char text[96];
  snprintf(text, sizeof(text), tr(STR_GAME_MINES_COUNTS), game.mineCount(), game.flags(), game.opened());
  center(top + 4, text, UI_10_FONT_ID);
  center(top + 32,
         game.result == Minesweeper::Won    ? tr(STR_GAME_MINES_WON)
         : game.result == Minesweeper::Lost ? tr(STR_GAME_MINES_LOST)
         : game.generated                   ? tr(STR_GAME_MINES_PLAY)
                                            : tr(STR_GAME_MINES_READY),
         SMALL_FONT_ID);
  for (int i = 0; i < game.count(); ++i) {
    const int x = boardX + i % game.width() * unit, y = boardY + i / game.width() * unit;
    const auto cell = game.cells[i];
    const bool mine = (cell & Minesweeper::MINE) && game.result != Minesweeper::Running;
    renderer.drawRect(x, y, unit, unit);
    if (!(cell & Minesweeper::OPEN) && !mine) renderer.drawRect(x + 3, y + 3, unit - 6, unit - 6);
    if (i == game.exploded) renderer.fillRect(x + 2, y + 2, unit - 4, unit - 4);
    const bool black = i != game.exploded;
    const int cx = x + unit / 2, cy = y + unit / 2;
    if (mine) {
      renderer.fillRect(cx - 3, cy - 3, 7, 7, black);
      renderer.drawLine(cx - 7, cy, cx + 7, cy, black);
      renderer.drawLine(cx, cy - 7, cx, cy + 7, black);
    } else if (cell & Minesweeper::FLAG) {
      renderer.drawLine(cx - 4, cy - 7, cx - 4, cy + 7);
      renderer.fillRect(cx - 3, cy - 7, 9, 6);
      if (game.result == Minesweeper::Lost) {
        renderer.drawLine(x + 5, y + 5, x + unit - 5, y + unit - 5);
        renderer.drawLine(x + unit - 5, y + 5, x + 5, y + unit - 5);
      }
    } else if (cell & Minesweeper::OPEN) {
      const int number = game.adjacent(i);
      if (number) {
        snprintf(text, sizeof(text), "%d", number);
        const int font = unit >= 40 ? UI_12_FONT_ID : SMALL_FONT_ID;
        UITheme::drawCenteredText(renderer, Rect{x, y, unit, unit}, font, cy - (unit >= 40 ? 12 : 8), text);
      }
    }
    if (i == cursor && game.result == Minesweeper::Running)
      renderer.drawRect(x + 1, y + 1, unit - 2, unit - 2, 2, true);
  }
  const int bottom = boardY + unit * game.width();
  renderer.drawRect(safe.x + 12, bottom + 12, safe.width - 24, 44, 2, true);
  center(bottom + 22, flagMode ? tr(STR_GAME_MINES_FLAG) : tr(STR_GAME_MINES_REVEAL), UI_10_FONT_ID);
  center(bottom + 64, tr(STR_GAME_MINES_HINT), SMALL_FONT_ID);
  center(bottom + 88, tr(STR_GAME_MINES_KEY_HINT), SMALL_FONT_ID);
}
}  // namespace games
