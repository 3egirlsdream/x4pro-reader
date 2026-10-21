#include <Memory.h>

#include "AppStorage.h"
#include "NativeApp.h"
#include "OpdsServerStore.h"
#include "components/UiAppHelpers.h"
void requestPowerOff();
namespace {
class AppsMenuActivity final : public NativeAppActivity {
 public:
  AppsMenuActivity(GfxRenderer& r, MappedInputManager& input)
      : NativeAppActivity("apps", StrId::STR_APP_APPS, r, input) {}

 protected:
  int listCount() const override { return nativeAppCount() + 6; }
  void onBackButton() override { activityManager.goHome(HomeMenuItem::APPS); }
  void buildScreen(UiScreen& screen) override {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
    screen.setContentMarginFromScreen(freeink::ui::Insets{
        int16_t(safe.y + metrics.topPadding + metrics.headerHeight),
        int16_t(renderer.getScreenWidth() - safe.x - safe.width),
        int16_t(renderer.getScreenHeight() - safe.y - safe.height + metrics.buttonHintsHeight), int16_t(safe.x)});
    freeink::ui::ListProps props;
    props.count = listCount();
    props.action = ACTION_ROW;
    props.inputMask = freeink::ui::InputTouch;
    props.rowProviderCtx = this;
    props.rowProvider = [](void* context, uint16_t index, freeink::ui::ListItem& item) {
      static_cast<AppsMenuActivity*>(context)->provide(index, item);
    };
    syncListViewport(screen, props);
    screen.list(props);
  }
  void provide(int index, freeink::ui::ListItem& item) {
    item = {};
    item.actionValue = index;
    if (size_t(index) < nativeAppCount()) {
      const auto& entry = nativeApps()[index];
      bool valid = appStorage::validId(entry.id);
      for (int j = 0; j < index; ++j)
        if (!strcmp(entry.id, nativeApps()[j].id)) valid = false;
      item.label = I18N.get(entry.title);
      item.icon = listIconFor(entry.icon, 32);
      item.enabled = valid;
    } else {
      const int system = index - nativeAppCount();
      static constexpr StrId titles[] = {StrId::STR_LIBRARY,        StrId::STR_BROWSE_FILES, StrId::STR_FILE_TRANSFER,
                                         StrId::STR_SETTINGS_TITLE, StrId::STR_OPDS_BROWSER, StrId::STR_APP_POWER_OFF};
      item.label = I18N.get(titles[system]);
      item.enabled = system != 4 || OPDS_STORE.hasServers();
    }
  }
  void activateIndex(int index) override {
    if (index < 0 || index >= listCount()) return;
    app.clearTapFlash();
    if (size_t(index) < nativeAppCount()) {
      freeink::ui::ListItem item;
      provide(index, item);
      if (!item.enabled) return;
      auto activity = nativeApps()[index].create(renderer, mappedInput);
      if (activity) activityManager.replaceActivity(std::move(activity));
      return;
    }
    switch (index - nativeAppCount()) {
      case 0:
        activityManager.goToLibrary();
        break;
      case 1:
        activityManager.goToFileBrowser();
        break;
      case 2:
        activityManager.goToFileTransfer();
        break;
      case 3:
        activityManager.goToSettings();
        break;
      case 4:
        if (OPDS_STORE.hasServers()) activityManager.goToBrowser();
        break;
      case 5:
        requestPowerOff();
        break;
    }
  }
};
}  // namespace
void openApps(GfxRenderer& renderer, MappedInputManager& input) {
  auto menu = makeUniqueNoThrow<AppsMenuActivity>(renderer, input);
  if (!menu) {
    LOG_ERR("APP", "Cannot allocate apps menu");
    return;
  }
  activityManager.replaceActivity(std::move(menu));
}
