#include "WeatherActivity.h"

#include <HalDisplay.h>
#include <HalFrontlight.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>

#include <cmath>
#include <ctime>

#include "CrossPointState.h"
#include "SilentRestart.h"
#include "WeatherSleep.h"
#include "WeatherStore.h"
#include "WeatherTouchLayout.h"
#include "WeatherView.h"
#include "components/UITheme.h"
#include "fontIds.h"

extern bool deepSleepInProgress;

namespace weather {

void WeatherActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock;
    previousOrientation = renderer.getOrientation();
    renderer.setOrientation(GfxRenderer::LandscapeCounterClockwise);
  }
  if (unattended) {
    sleepActive = true;
    Frontlight.setOn(false);
    Store::instance().checkBattery(false);
  }
  entryPowerReleased = !mappedInput.isPressed(MappedInputManager::Button::Power);
  if (!refreshOnEntry) requestUpdateAndWait();
  auto& store = Store::instance();
  if (unattended && store.settings.batteryPaused)
    completed = true;
  else if (refreshOnEntry || (unattended && !fresh(store.snapshot, store.settings, time(nullptr))))
    refresh();
  else
    completed = true;
}
void WeatherActivity::onExit() {
  request.reset();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  renderer.setOrientation(previousOrientation);
  if (unattended) {
    sleepActive = false;
    deepSleepInProgress = false;
  }
  Activity::onExit();
}
void WeatherActivity::refresh() {
  const auto* city = Store::instance().city();
  if (!city) return;
  request = makeUniqueNoThrow<Request>();
  if (!request) {
    LOG_ERR("WX", "Cannot allocate bounded weather request");
    result = UpdateResult::NetworkError;
    completed = true;
    if (!unattended) requestUpdate();
    return;
  }
  request->begin(*city);
  result = UpdateResult::Running;
  completed = false;
  if (!unattended) requestUpdate();
}
void WeatherActivity::leave(bool home) {
  request.reset();
  if (unattended) {
    loadInteractiveStores();
    APP_STATE.weatherSleepArmed = false;
    APP_STATE.saveToFile();
    if (!home && APP_STATE.lastSleepFromReader && !APP_STATE.openEpubPath.empty())
      activityManager.goToReader(APP_STATE.openEpubPath);
    else
      activityManager.goHome();
  } else
    finish();
}
bool WeatherActivity::handleHomeGesture() {
  leave(true);
  return true;
}
void WeatherActivity::loop() {
  if (!mappedInput.isPressed(MappedInputManager::Button::Power)) entryPowerReleased = true;
  const bool power = unattended && entryPowerReleased && mappedInput.wasPressed(MappedInputManager::Button::Power);
  if (power || mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    leave();
    return;
  }
  if (!unattended) {
    const int width = renderer.getScreenWidth(), height = renderer.getScreenHeight();
    const int padding = UITheme::getInstance().getMetrics().contentSidePadding;
    const auto back = footerButtonRect(width, height, padding, false);
    const auto update = footerButtonRect(width, height, padding, true);
    if (mappedInput.wasNavigationTapInRect(back.x, back.y, back.width, back.height)) {
      leave();
      return;
    }
    if (!request && (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
                     mappedInput.wasNavigationTapInRect(update.x, update.y, update.width, update.height)))
      refresh();
  }
  if (request) {
    const auto next = request->poll();
    if (next != UpdateResult::Running) {
      auto diagnostic = request->diagnostic();
      {
        RenderLock lock;
        result = next;
        if (result == UpdateResult::Success && !Store::instance().acceptSnapshot(request->value()))
          result = UpdateResult::StorageError;
        diagnostic.result = result.load();
        ++frameRevision;
        request.reset();
        completed = true;
      }
      if (!Store::instance().saveDiagnostic(diagnostic)) LOG_ERR("WX", "Cannot save update diagnostic");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      if (!unattended) requestUpdate();
    }
  }
  if (unattended && completed) {
    Store::instance().checkBattery(false);
    if (refreshOnEntry || result != UpdateResult::Idle) requestUpdateAndWait();
    const auto& s = Store::instance().settings;
    uint32_t seconds = s.intervalMinutes * 60;
    const auto now = time(nullptr);
    const auto& cached = Store::instance().snapshot;
    if (result == UpdateResult::Idle && now >= cached.fetchedAt && now - cached.fetchedAt < seconds)
      seconds -= now - cached.fetchedAt;
    finishSleep(s.enabled && !s.batteryPaused ? seconds : 0);
  }
}
void WeatherActivity::render(RenderLock&&) {
  const auto& store = Store::instance();
  const auto status = result.load();
  if (!frameGate.claim(status, store.settings.batteryPaused, frameRevision)) return;
  display.setInverted(false);
  const View view{store.city(), &store.snapshot, store.hasCache, store.settings.batteryPaused, unattended, result};
  drawWeatherView(renderer, view);
  renderer.displayBuffer(status == UpdateResult::Running ? HalDisplay::FAST_REFRESH : HalDisplay::FULL_REFRESH);
}
}  // namespace weather
