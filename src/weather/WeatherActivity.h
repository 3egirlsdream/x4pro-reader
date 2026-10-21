#pragma once
#include <atomic>
#include <memory>

#include "WeatherRequest.h"
#include "activities/Activity.h"

namespace weather {
class WeatherActivity final : public Activity {
 public:
  WeatherActivity(GfxRenderer& r, MappedInputManager& input, bool unattended, bool refresh)
      : Activity("Weather", r, input), unattended(unattended), refreshOnEntry(refresh) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return unattended || request != nullptr; }
  bool handleHomeGesture() override;

 private:
  const bool unattended;
  const bool refreshOnEntry;
  bool completed = false;
  bool entryPowerReleased = false;
  FrameGate frameGate;
  std::atomic<UpdateResult> result{UpdateResult::Idle};
  GfxRenderer::Orientation previousOrientation = GfxRenderer::Portrait;
  std::unique_ptr<Request> request;
  void refresh();
  void leave(bool home = false);
};
}  // namespace weather
