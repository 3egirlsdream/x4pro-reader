#include <Memory.h>

#include "CounterActivity.h"
#include "DeviceInfoActivity.h"
#include "NativeApp.h"
#include "games/GameActivity.h"
#include "games/MinesweeperActivity.h"
#include "weather/WeatherMenuActivity.h"

namespace {
template <class T>
std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& input) {
  // Activities require owning dynamic lifetime; no allocation is performed in loop().
  auto value = makeUniqueNoThrow<T>(renderer, input);
  if (!value) LOG_ERR("APP", "Cannot allocate application");
  return value;
}
constexpr NativeAppDescriptor APPS[] = {
    {"weather", StrId::STR_APP_WEATHER, UIIcon::Blocks, create<weather::MenuActivity>},
    {"game-2048", StrId::STR_GAME_2048, UIIcon::Blocks, create<games::Game2048Activity>},
    {"game-gomoku", StrId::STR_GAME_GOMOKU, UIIcon::Blocks, create<games::GomokuActivity>},
    {"game-mines", StrId::STR_GAME_MINES, UIIcon::Blocks, create<games::MinesweeperActivity>},
    {"counter", StrId::STR_APP_COUNTER, UIIcon::Bookmark, create<CounterActivity>},
    {"device-info", StrId::STR_APP_DEVICE_INFO, UIIcon::Settings, create<DeviceInfoActivity>},
};
}  // namespace
const NativeAppDescriptor* nativeApps() { return APPS; }
size_t nativeAppCount() { return sizeof(APPS) / sizeof(APPS[0]); }
