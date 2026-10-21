#pragma once
#include "GameActivity.h"
#include "Minesweeper.h"
namespace games {
class MinesweeperActivity final : public GameActivity {
 public:
  MinesweeperActivity(GfxRenderer& r, MappedInputManager& input)
      : GameActivity("game-mines", StrId::STR_GAME_MINES, r, input) {}

 private:
  Minesweeper game;
  int cursor = 40;
  bool flagMode = false;
  int boardDivisions() const override { return game.width(); }
  int menuCount() const override { return 4; }
  const char* menuLabel(int index) const override;
  void activateMenu(int index) override;
  size_t activityBytes() const override { return sizeof(*this); }
  void load() override;
  void save() override;
  void drawGame() override;
  void inputGame() override;
  void newGame(bool alternate) override;
  int hit(int x, int y) const;
  void act();
};
}  // namespace games
