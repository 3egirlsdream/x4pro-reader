#pragma once
#include <array>

#include "HomeCoverCache.h"
#include "UiAppHost.h"
#include "desktop/DesktopShelf.h"

class CoverGridHomeUi final : public UiAppHost {
 public:
  explicit CoverGridHomeUi(GfxRenderer& renderer);
  void begin(const desktop::Shelf& shelf, bool opds);
  void setSelection(int value) { selected = value; }
  void pageChanged() { coverCache.invalidate(); }
  int selectedAction(const MappedInputManager& input);
  int appCount() const;
  int thumbHeight() const { return thumbHeightValue; }
  void activateApp(int index, MappedInputManager& input);

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
  void draw(UiScreen& screen);
  GfxRenderer& renderer;
  HomeCoverCache coverCache;
  const desktop::Shelf* shelf = nullptr;
  bool opds = false;
  int selected = 0, pending = -1;
  int thumbHeightValue = 0;
};
