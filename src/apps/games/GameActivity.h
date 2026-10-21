#pragma once
#include <I18n.h>

#include "Game2048.h"
#include "Gomoku.h"
#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"
namespace games {
class GameActivity : public Activity {
 public:
  GameActivity(const char* id, StrId title, GfxRenderer& r, MappedInputManager& input)
      : Activity(id, r, input), id(id), title(title) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleForcedRefresh() override;

 protected:
  const char* id;
  StrId title;
  bool changed = false;
  uint32_t revision = 1, drawn = 0;
  unsigned boardUpdates = 0;
  bool menu = false;
  bool forceFull = false;
  int menuIndex = 0;
  Rect safe{};
  int top = 0, boardX = 0, boardY = 0, unit = 0, footer = 0;
  uint8_t binary[Gomoku::SAVE_SIZE]{};
  char encoded[Gomoku::SAVE_SIZE * 2 + 1]{};
  virtual void load() = 0;
  virtual void save() = 0;
  virtual void drawGame() = 0;
  virtual void inputGame() = 0;
  virtual void newGame(bool alternate) = 0;
  virtual void stopWork() {}
  virtual void resumeWork() {}
  virtual bool gomoku() const { return false; }
  virtual int boardDivisions() const { return gomoku() ? 14 : 4; }
  virtual int menuCount() const { return gomoku() ? 3 : 2; }
  virtual const char* menuLabel(int index) const;
  virtual void activateMenu(int index);
  virtual size_t activityBytes() const { return sizeof(*this); }
  void update(bool board = false);
  void layout();
  void center(int y, const char* text, int font) const;

 private:
  GfxRenderer::Orientation previous = GfxRenderer::Portrait;
};
class Game2048Activity final : public GameActivity {
 public:
  Game2048Activity(GfxRenderer& r, MappedInputManager& input)
      : GameActivity("game-2048", StrId::STR_GAME_2048, r, input) {}

 private:
  size_t activityBytes() const override { return sizeof(*this); }
  Game2048 game;
  bool horizontal = false;
  void load() override;
  void save() override;
  void drawGame() override;
  void inputGame() override;
  void newGame(bool alternate) override;
};
class GomokuActivity final : public GameActivity {
 public:
  GomokuActivity(GfxRenderer& r, MappedInputManager& input)
      : GameActivity("game-gomoku", StrId::STR_GAME_GOMOKU, r, input) {}
  bool skipLoopDelay() override { return ai.running(); }

 private:
  size_t activityBytes() const override { return sizeof(*this); }
  Gomoku game;
  GomokuAI ai;
  int cursor = 112;
  static constexpr unsigned SLICE_NODES = 32;
  static constexpr uint32_t SLICE_US = 2000;
  static constexpr uint32_t COMPUTE_LIMIT_US = 200000;
  static constexpr uint32_t WALL_LIMIT_MS = 3000;
  uint32_t computeUs = 0;
  uint32_t searchStartedMs = 0;
  bool gomoku() const override { return true; }
  void load() override;
  void save() override;
  void drawGame() override;
  void inputGame() override;
  void newGame(bool alternate) override;
  void stopWork() override { ai.cancel(); }
  void resumeWork() override;
  void place();
};
}  // namespace games
