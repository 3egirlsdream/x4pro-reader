#include "NativeApp.h"

#include <Memory.h>

#include "AppStorage.h"
#include "components/UITheme.h"

NativeAppActivity::NativeAppActivity(const char* id, StrId title, GfxRenderer& r, MappedInputManager& input)
    : UiListActivity(id, r, input), appId(id), titleId(title) {}
const char* NativeAppActivity::headerTitle() const { return I18N.get(titleId); }
bool NativeAppActivity::loadData(JsonDocument& doc) const { return appStorage::load(appId, "state", doc); }
bool NativeAppActivity::saveData(const JsonDocument& doc) const { return appStorage::save(appId, "state", doc); }
void NativeAppActivity::onBackButton() { openApps(renderer, mappedInput); }
void NativeAppActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(freeink::ui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
      static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height + metrics.buttonHintsHeight),
      static_cast<int16_t>(safe.x)});
  screen.spacer(metrics.verticalSpacing);
  freeink::ui::ListProps props;
  props.items = rows.data();
  props.count = rowCount;
  props.action = ACTION_ROW;
  props.inputMask = freeink::ui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
