#pragma once
#include "apps/NativeApp.h"

// Copy to src/apps/HelloToolActivity.h, add translation keys and a registry entry.
class HelloToolActivity final : public NativeAppActivity {
 public:
  HelloToolActivity(GfxRenderer& renderer, MappedInputManager& input)
      : NativeAppActivity("hello-tool", StrId::STR_APP_HELLO_TOOL, renderer, input) {}
  void onEnter() override {
    rowCount = 1;
    rows[0].label = tr(STR_BACK);
    NativeAppActivity::onEnter();
  }

 protected:
  void activateIndex(int index) override {
    if (index == 0) onBackButton();
  }
};
