#pragma once
#include "apps/NativeApp.h"

namespace weather {
class MenuActivity final : public NativeAppActivity {
 public:
  MenuActivity(GfxRenderer& r, MappedInputManager& input)
      : NativeAppActivity("weather", StrId::STR_APP_WEATHER, r, input) {}
  void onEnter() override;
  void onExit() override;

 protected:
  void activateIndex(int index) override;
  void buildScreen(UiScreen& screen) override;

 private:
  bool needsSetupMessage = false;
  char interval[24]{};
  char coordinateText[80]{};
  float editedLatitude = 0;
  float editedLongitude = 0;
  bool settingsSaveFailed = false;
  bool coordinateInvalid = false;
  void editCoordinates(int step);
};
}  // namespace weather
