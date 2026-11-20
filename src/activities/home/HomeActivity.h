#pragma once
#include "activities/Activity.h"
#include "activities/InputQueue.h"
#include "components/CoverGridHomeUi.h"
#include "desktop/DesktopShelf.h"
class HomeActivity final : public Activity {
  desktop::Shelf shelf;
  std::unique_ptr<CoverGridHomeUi> ui;
  HomeMenuItem initial;
  bool cleanInitialRefresh;
  std::atomic<bool> firstRenderDone{false};
  int selection = 0;
  InputQueue<int> commands;
  uint32_t pendingSince = 0;
  void processCommand(int command);
  uint32_t minute = 0;
  void activate(int action);
  void changePage(int delta);

 public:
  HomeActivity(GfxRenderer& renderer, MappedInputManager& input, HomeMenuItem item = HomeMenuItem::NONE,
               bool clean = false)
      : Activity("Home", renderer, input), initial(item), cleanInitialRefresh(clean) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
};
