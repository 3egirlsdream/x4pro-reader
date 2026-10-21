#pragma once
#include "NativeApp.h"

class CounterActivity final : public NativeAppActivity {
 public:
  CounterActivity(GfxRenderer& r, MappedInputManager& input)
      : NativeAppActivity("counter", StrId::STR_APP_COUNTER, r, input) {}
  void onEnter() override;
  void onExit() override;

 protected:
  void activateIndex(int index) override;

 private:
  int32_t value = 0;
  int32_t savedValue = 0;
  char valueText[24]{};
  void updateRows();
};
