#pragma once
#include <ArduinoJson.h>
#include <I18n.h>

#include <array>

#include "activities/UiListActivity.h"
#include "components/themes/BaseTheme.h"

class NativeAppActivity : public UiListActivity {
 public:
  NativeAppActivity(const char* id, StrId title, GfxRenderer& renderer, MappedInputManager& input);
  bool loadData(JsonDocument& doc) const;
  bool saveData(const JsonDocument& doc) const;

 protected:
  const char* appId;
  StrId titleId;
  std::array<freeink::ui::ListItem, 12> rows{};
  uint16_t rowCount = 0;
  const char* headerTitle() const override;
  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void onBackButton() override;
};

struct NativeAppDescriptor {
  const char* id;
  StrId title;
  UIIcon icon;
  std::unique_ptr<Activity> (*create)(GfxRenderer&, MappedInputManager&);
};
const NativeAppDescriptor* nativeApps();
size_t nativeAppCount();
void openApps(GfxRenderer& renderer, MappedInputManager& input);
