#include "WeatherStore.h"

#include <ArduinoJson.h>
#include <HalPowerManager.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "WeatherJson.h"
#include "activities/RenderLock.h"
#include "apps/AppStorage.h"

namespace weather {
namespace {
bool validCache(JsonVariantConst doc) {
  Snapshot value;
  return snapshotFromJson(doc, value);
}
bool validSettings(JsonVariantConst doc) {
  const auto city = doc["cityId"] | uint32_t(0);
  const int32_t latitude = doc["latitudeE5"] | int32_t(99900000);
  const int32_t longitude = doc["longitudeE5"] | int32_t(99900000);
  const char* name = doc["locationName"] | "";
  const bool custom = coordinateLocationIdE5(latitude, longitude) != 0 && *name && strlen(name) < 64 &&
                      coordinateLocationIdE5(latitude, longitude) == city;
  return (doc["schema"] | 0) == 1 && (city == 0 || findCity(city) || custom) &&
         validInterval(doc["intervalMinutes"] | uint16_t(0));
}
}  // namespace
Store& Store::instance() {
  static Store store;
  return store;
}
void Store::load() {
  settings = {};
  hasCache = false;
  customCity = {};
  customName[0] = 0;
  JsonDocument doc;
  if (appStorage::load("weather", "settings", doc, validSettings)) {
    settings.cityId = doc["cityId"] | uint32_t(0);
    settings.intervalMinutes = doc["intervalMinutes"] | uint16_t(30);
    settings.enabled = doc["enabled"] | false;
    settings.batteryPaused = doc["batteryPaused"] | false;
    if (!findCity(settings.cityId) && settings.cityId) {
      snprintf(customName, sizeof(customName), "%s", doc["locationName"] | "");
      customLatitudeE5 = doc["latitudeE5"] | int32_t(0);
      customLongitudeE5 = doc["longitudeE5"] | int32_t(0);
      customCity = {settings.cityId, "", customName, customLatitudeE5 / 100000.0f, customLongitudeE5 / 100000.0f};
    }
    if (!validInterval(settings.intervalMinutes)) settings.intervalMinutes = 30;
    if (!city()) settings = {};
  }
  doc.clear();
  if (appStorage::load("weather", "cache", doc, validCache))
    hasCache = snapshotFromJson(doc.as<JsonVariantConst>(), snapshot) && snapshot.cityId == settings.cityId;
  if (!hasCache) settings.enabled = false;
}
static bool writeSettings(const Settings& settings, const char* name, int32_t latitude, int32_t longitude) {
  JsonDocument doc;
  doc["schema"] = 1;
  doc["cityId"] = settings.cityId;
  doc["intervalMinutes"] = settings.intervalMinutes;
  doc["enabled"] = settings.enabled;
  doc["batteryPaused"] = settings.batteryPaused;
  if (name) {
    doc["locationName"] = name;
    doc["latitudeE5"] = latitude;
    doc["longitudeE5"] = longitude;
  }
  return appStorage::save("weather", "settings", doc);
}
bool Store::saveSettings() const {
  return writeSettings(settings, customCity.id && customCity.id == settings.cityId ? customName : nullptr,
                       customLatitudeE5, customLongitudeE5);
}
bool Store::acceptSnapshot(const Snapshot& value) {
  if (value.cityId != settings.cityId || !validSnapshot(value)) return false;
  JsonDocument doc;
  snapshotToJson(value, doc);
  if (!appStorage::save("weather", "cache", doc)) return false;
  snapshot = value;
  hasCache = true;
  return true;
}
bool Store::canEnable() const { return city() && hasCache && validSnapshot(snapshot); }
bool Store::saveDiagnostic(const UpdateDiagnostic& value) const {
  JsonDocument doc;
  doc["schema"] = 1;
  doc["stage"] = static_cast<unsigned>(value.stage);
  doc["result"] = static_cast<unsigned>(value.result);
  doc["elapsedMs"] = value.elapsedMs;
  doc["error"] = value.error;
  doc["tlsError"] = value.tlsError;
  doc["tlsCode"] = value.tlsCode;
  doc["tlsFlags"] = value.tlsFlags;
  doc["httpStatus"] = value.httpStatus;
  doc["fallback"] = value.fallback;
  return appStorage::save("weather", "diagnostic", doc);
}
const City* Store::city() const {
  return customCity.id && customCity.id == settings.cityId ? &customCity : findCity(settings.cityId);
}
bool Store::setEnabled(bool enabled) {
  if (enabled && !canEnable()) return false;
  const bool previous = settings.enabled;
  settings.enabled = enabled;
  if (saveSettings()) return true;
  settings.enabled = previous;
  return false;
}
bool Store::selectCoordinates(const char* name, float latitude, float longitude) {
  if (!name || !*name || strlen(name) >= sizeof(customName) || !validCoordinates(latitude, longitude)) return false;
  const int32_t lat = std::lround(double(latitude) * 100000);
  const int32_t lon = std::lround(double(longitude) * 100000);
  const uint32_t id = coordinateLocationIdE5(lat, lon);
  Settings next = settings;
  next.cityId = id;
  if (id != settings.cityId) next.enabled = false;
  if (!writeSettings(next, name, lat, lon)) return false;
  customLatitudeE5 = lat;
  customLongitudeE5 = lon;
  snprintf(customName, sizeof(customName), "%s", name);
  customCity = {id, "", customName, latitude, longitude};
  if (id != settings.cityId) {
    hasCache = false;
    snapshot = {};
  }
  settings = next;
  return true;
}
bool Store::selectCity(uint32_t id) {
  if (!findCity(id)) return false;
  if (id == settings.cityId) return true;
  Settings next = settings;
  next.cityId = id;
  next.enabled = false;
  if (!writeSettings(next, nullptr, 0, 0)) return false;
  settings = next;
  customCity = {};
  hasCache = false;
  snapshot = {};
  return true;
}
void Store::checkBattery(bool userWake) {
  uint16_t percent = 0;
  const bool known = powerManager.readBatteryPercentageChecked(percent);
  RenderLock lock;
  const bool paused = pauseForBattery(settings.batteryPaused, known, percent, userWake);
  if (paused != settings.batteryPaused) {
    settings.batteryPaused = paused;
    saveSettings();
  }
}
}  // namespace weather
