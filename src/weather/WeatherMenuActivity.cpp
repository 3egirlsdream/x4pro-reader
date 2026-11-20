#include "WeatherMenuActivity.h"

#include <Memory.h>
#include <WiFi.h>

#include "WeatherActivity.h"
#include "WeatherStore.h"
#include "WifiCredentialStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace weather {
namespace {
class CityPicker final : public UiListActivity {
  bool saveFailed = false;

 public:
  CityPicker(GfxRenderer& r, MappedInputManager& input) : UiListActivity("WeatherCities", r, input) {}

 protected:
  const char* selectedProvince = nullptr;
  int listCount() const override {
    int count = 0;
    const char* last = nullptr;
    for (size_t i = 0; i < cityCount(); ++i) {
      const auto& city = cities()[i];
      if (selectedProvince) {
        if (!strcmp(city.province, selectedProvince)) ++count;
      } else if (!last || strcmp(city.province, last)) {
        ++count;
        last = city.province;
      }
    }
    return count;
  }
  const City* entry(int index) const {
    const char* last = nullptr;
    for (size_t i = 0; i < cityCount(); ++i) {
      const auto& city = cities()[i];
      if (selectedProvince) {
        if (!strcmp(city.province, selectedProvince) && index-- == 0) return &city;
      } else if (!last || strcmp(city.province, last)) {
        last = city.province;
        if (index-- == 0) return &city;
      }
    }
    return nullptr;
  }
  const char* headerTitle() const override {
    return saveFailed ? tr(STR_SAVE_PROGRESS_FAILED) : selectedProvince ? selectedProvince : tr(STR_WX_CITY);
  }
  void onBackButton() override {
    if (selectedProvince) {
      selectedProvince = nullptr;
      nav.reset();
      requestUpdate();
    } else
      finish();
  }
  void activateIndex(int index) override {
    const auto* city = entry(index);
    if (!city) return;
    if (!selectedProvince) {
      selectedProvince = city->province;
      nav.reset();
      requestUpdate();
    } else {
      {
        RenderLock lock;
        saveFailed = !Store::instance().selectCity(city->id);
      }
      if (saveFailed)
        requestUpdate();
      else
        finish();
    }
  }
  void buildScreen(UiScreen& screen) override {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
    screen.setContentMarginFromScreen(freeink::ui::Insets{
        static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
        static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
        static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height + metrics.buttonHintsHeight),
        static_cast<int16_t>(safe.x)});
    freeink::ui::ListProps props;
    props.count = listCount();
    props.action = ACTION_ROW;
    props.inputMask = freeink::ui::InputTouch;
    props.rowProviderCtx = this;
    props.rowProvider = [](void* ctx, uint16_t index, freeink::ui::ListItem& row) {
      auto* self = static_cast<CityPicker*>(ctx);
      const auto* city = self->entry(index);
      row.label = city ? (self->selectedProvince ? city->name : city->province) : "";
      row.actionValue = index;
    };
    syncListViewport(screen, props);
    screen.list(props);
  }
};
}  // namespace
void MenuActivity::onEnter() {
  Store::instance().load();
  Store::instance().checkBattery(true);
  rowCount = 8;
  rows[0].label = tr(STR_WX_VIEW);
  rows[1].label = tr(STR_WX_REFRESH);
  rows[2].label = tr(STR_WX_CITY);
  rows[3].label = tr(STR_WX_PRECISE_LOCATION);
  rows[4].label = tr(STR_WX_WIFI);
  rows[5].label = tr(STR_WX_AUTO);
  rows[5].toggle = true;
  rows[6].label = tr(STR_WX_INTERVAL);
  rows[7].label = tr(STR_WX_SOURCE);
  rows[7].enabled = false;
  for (int i = 0; i < rowCount; ++i) rows[i].actionValue = i;
  NativeAppActivity::onEnter();
}
void MenuActivity::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  NativeAppActivity::onExit();
}
void MenuActivity::buildScreen(UiScreen& screen) {
  const auto& store = Store::instance();
  rows[2].value = store.city() ? store.city()->name : tr(STR_WX_NOT_SET);
  if (const auto* city = store.city())
    snprintf(coordinateText, sizeof(coordinateText), "WGS84 %.5f, %.5f", city->latitude, city->longitude);
  else
    coordinateText[0] = 0;
  rows[3].subtitle = coordinateInvalid ? tr(STR_WX_COORDINATE_INVALID) : coordinateText;
  rows[5].toggleChecked = store.settings.enabled;
  rows[5].subtitle = settingsSaveFailed             ? tr(STR_SAVE_PROGRESS_FAILED)
                     : needsSetupMessage            ? tr(STR_WX_SETUP_REQUIRED)
                     : store.settings.batteryPaused ? tr(STR_WX_BATTERY_PAUSED)
                     : store.settings.enabled       ? tr(STR_WX_ENABLED)
                                                    : tr(STR_WX_DISABLED);
  snprintf(interval, sizeof(interval), tr(STR_WX_MINUTES), store.settings.intervalMinutes);
  rows[6].value = interval;
  NativeAppActivity::buildScreen(screen);
}
void MenuActivity::activateIndex(int index) {
  auto& store = Store::instance();
  if (index == 0 || index == 1) {
    if (!store.city()) {
      needsSetupMessage = true;
      requestUpdate();
      return;
    }
    auto view = makeUniqueNoThrow<WeatherActivity>(renderer, mappedInput, false, index == 1);
    if (view) startActivityForResult(std::move(view), [](const ActivityResult&) {});
  } else if (index == 2) {
    auto picker = makeUniqueNoThrow<CityPicker>(renderer, mappedInput);
    if (picker) startActivityForResult(std::move(picker), [](const ActivityResult&) {});
  } else if (index == 3) {
    editCoordinates(0);
  } else if (index == 4) {
    auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, false, true);
    if (wifi) startActivityForResult(std::move(wifi), [this](const ActivityResult&) { requestUpdate(); });
  } else if (index == 5) {
    WIFI_STORE.loadFromFile();
    RenderLock lock;
    needsSetupMessage = false;
    settingsSaveFailed = false;
    if (store.settings.enabled)
      settingsSaveFailed = !store.setEnabled(false);
    else if (store.canEnable() && WIFI_STORE.getCredentialCount() != 0)
      settingsSaveFailed = !store.setEnabled(true);
    else
      needsSetupMessage = true;
  } else if (index == 6) {
    RenderLock lock;
    store.settings.intervalMinutes = store.settings.intervalMinutes == 15   ? 30
                                     : store.settings.intervalMinutes == 30 ? 60
                                                                            : 15;
    store.saveSettings();
  }
  requestUpdate();
}
void MenuActivity::editCoordinates(int step) {
  const auto* current = Store::instance().city();
  char initial[64]{};
  if (step == 0 && current) snprintf(initial, sizeof(initial), "%.5f", current->latitude);
  if (step == 1 && current) snprintf(initial, sizeof(initial), "%.5f", current->longitude);
  if (step == 2) snprintf(initial, sizeof(initial), "%s", current ? current->name : tr(STR_WX_PRECISE_LOCATION));
  const char* title = step == 0 ? tr(STR_WX_LATITUDE) : step == 1 ? tr(STR_WX_LONGITUDE) : tr(STR_WX_LOCATION_NAME);
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, initial, size_t(63));
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this, step](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!text) return;
    float coordinate = 0;
    if (step < 2 && !parseCoordinate(text->text.c_str(), step == 0 ? -90 : -180, step == 0 ? 90 : 180, coordinate)) {
      coordinateInvalid = true;
      requestUpdate();
      return;
    }
    coordinateInvalid = false;
    if (step == 0) {
      editedLatitude = coordinate;
      editCoordinates(1);
    } else if (step == 1) {
      editedLongitude = coordinate;
      editCoordinates(2);
    } else {
      RenderLock lock;
      settingsSaveFailed = !Store::instance().selectCoordinates(text->text.c_str(), editedLatitude, editedLongitude);
      requestUpdate();
    }
  });
}
}  // namespace weather
