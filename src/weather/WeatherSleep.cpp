#include "WeatherSleep.h"

#include <BoardConfig.h>
#include <HalStorage.h>
#include <Memory.h>

#include "CrossPointState.h"
#include "WeatherActivity.h"
#include "WeatherStore.h"

extern bool deepSleepInProgress;

namespace weather {
bool sleepActive = false;
bool startSleep(GfxRenderer& renderer, MappedInputManager& input) {
  auto& store = Store::instance();
  if (!BoardConfig::isX4Pro() || !store.settings.enabled || !store.canEnable() || sleepActive) return false;
  auto activity = makeUniqueNoThrow<WeatherActivity>(renderer, input, true, false);
  if (!activity) {
    LOG_ERR("WX", "Cannot allocate weather sleep activity");
    return false;
  }
  APP_STATE.lastSleepFromReader = activityManager.isReaderActivity();
  APP_STATE.showBootScreen = false;
  APP_STATE.weatherSleepArmed = false;
  APP_STATE.saveToFile();
  // Stop outgoing network activities from requesting their maintenance reboot.
  deepSleepInProgress = true;
  sleepActive = true;
  Storage.remove("/.crosspoint/sleep_frame.bin");
  activityManager.replaceActivity(std::move(activity));
  activityManager.loop();
  return true;
}
}  // namespace weather
