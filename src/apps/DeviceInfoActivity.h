#pragma once
#include "NativeApp.h"

class DeviceInfoActivity final : public NativeAppActivity {
 public:
  DeviceInfoActivity(GfxRenderer& r, MappedInputManager& input)
      : NativeAppActivity("device-info", StrId::STR_APP_DEVICE_INFO, r, input) {}
  void onEnter() override {
    snprintf(dimensions, sizeof(dimensions), "%d x %d", renderer.getScreenWidth(), renderer.getScreenHeight());
    rowCount = 2;
    rows[0].label = tr(STR_APP_SCREEN_SIZE);
    rows[0].subtitle = dimensions;
    rows[1].label = tr(STR_BACK);
    rows[1].actionValue = 1;
    NativeAppActivity::onEnter();
  }

 protected:
  void activateIndex(int index) override {
    if (index == 1) onBackButton();
  }

 private:
  char dimensions[24]{};
};
