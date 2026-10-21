#include <HalPowerManager.h>
#include <HalStorage.h>

#include <cassert>
#include <iostream>

#include "apps/AppStorage.h"
#include "weather/WeatherStore.h"
FakeStorage testStorage;
HalPowerManager powerManager;
int main() {
  using namespace weather;
  auto& store = Store::instance();
  store.load();
  assert(!store.canEnable() && !store.setEnabled(true));
  assert(store.selectCoordinates("Home", 31.2304f, 121.4737f));
  const auto homeId = store.settings.cityId;
  Snapshot s;
  s.cityId = homeId;
  s.fetchedAt = s.dataTime = 1792454400;
  s.temperature = 24;
  s.humidity = 60;
  s.wind = 2;
  s.code = 3;
  s.dayCount = 5;
  for (size_t i = 0; i < 5; ++i) s.days[i] = {s.dataTime + int64_t(i) * 86400, 10, 25, 3, 50};
  assert(store.acceptSnapshot(s));
  assert(store.setEnabled(true));
  store.load();
  assert(store.settings.enabled && store.hasCache && store.city()->id == homeId);
  assert(std::string(store.city()->name) == "Home");
  assert(store.setEnabled(false));
  testStorage.renameCalls = 0;
  testStorage.failRename = 2;
  assert(!store.setEnabled(true) && !store.settings.enabled);
  testStorage.failRename = 0;
  assert(store.selectCoordinates("Office", 31.2404f, 121.4737f));
  assert(store.settings.cityId != homeId && !store.hasCache && !store.settings.enabled);
  store.load();
  assert(!store.hasCache && !store.settings.enabled && std::string(store.city()->name) == "Office");
  assert(!store.selectCoordinates("Bad", 91, 0));
  store.selectCity(cities()[0].id);
  store.load();
  assert(store.city()->id == cities()[0].id && !store.hasCache);
  powerManager.known = false;
  store.checkBattery(true);
  assert(store.settings.batteryPaused);
  powerManager.known = true;
  powerManager.percent = 80;
  store.checkBattery(false);
  assert(store.settings.batteryPaused);
  store.checkBattery(true);
  assert(!store.settings.batteryPaused);
  std::cout << "Weather Store: coordinate roundtrip, switch persistence/rollback, stale-location cache rejection and "
               "battery hysteresis passed\n";
}
