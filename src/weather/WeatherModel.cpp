#include "WeatherModel.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace weather {
namespace {
bool validCode(int code) {
  return code == 0 || code == 1 || code == 2 || code == 3 || code == 45 || code == 48 ||
         (code >= 51 && code <= 57 && code % 2 == 1) || code == 61 || code == 63 || code == 65 || code == 66 ||
         code == 67 || code == 71 || code == 73 || code == 75 || code == 77 || (code >= 80 && code <= 82) ||
         code == 85 || code == 86 || code == 95 || code == 96 || code == 99;
}
bool validTemperature(float v) { return std::isfinite(v) && v >= -100 && v <= 70; }
}  // namespace
bool validCoordinates(float latitude, float longitude) {
  return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90 && latitude <= 90 &&
         longitude >= -180 && longitude <= 180;
}
uint32_t coordinateLocationIdE5(int32_t latitude, int32_t longitude) {
  if (latitude < -9000000 || latitude > 9000000 || longitude < -18000000 || longitude > 18000000) return 0;
  uint32_t hash = 2166136261U;
  for (const int32_t value : {latitude, longitude}) {
    for (unsigned i = 0; i < 4; ++i) hash = (hash ^ (uint32_t(value) >> (i * 8) & 255U)) * 16777619U;
  }
  return hash | 0x80000000U;
}
uint32_t coordinateLocationId(float latitude, float longitude) {
  if (!validCoordinates(latitude, longitude)) return 0;
  return coordinateLocationIdE5(static_cast<int32_t>(std::round(double(latitude) * 100000)),
                                static_cast<int32_t>(std::round(double(longitude) * 100000)));
}
bool parseCoordinate(const char* text, float minimum, float maximum, float& value) {
  if (!text || !*text) return false;
  char* end = nullptr;
  const float parsed = std::strtof(text, &end);
  if (end == text || !std::isfinite(parsed) || parsed < minimum || parsed > maximum) return false;
  while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end) return false;
  value = parsed;
  return true;
}
bool validSnapshot(const Snapshot& v) {
  if (!v.cityId || v.dataTime < 1577836800 || v.fetchedAt < 1577836800 || !validTemperature(v.temperature) ||
      !std::isfinite(v.wind) || v.wind < 0 || v.wind > 150 || v.humidity < 0 || v.humidity > 100 ||
      !validCode(v.code) || !v.dayCount || v.dayCount > FORECAST_DAYS)
    return false;
  for (size_t i = 0; i < v.dayCount; ++i) {
    const auto& d = v.days[i];
    if (d.time < 1577836800 || (i && d.time <= v.days[i - 1].time) || !validTemperature(d.minimum) ||
        !validTemperature(d.maximum) || d.minimum > d.maximum || !validCode(d.code) || d.rainPercent < -1 ||
        d.rainPercent > 100)
      return false;
  }
  return true;
}
bool fresh(const Snapshot& v, const Settings& s, int64_t now) {
  return validInterval(s.intervalMinutes) && v.cityId == s.cityId && validSnapshot(v) && now >= v.fetchedAt &&
         now - v.fetchedAt < s.intervalMinutes * 60;
}
}  // namespace weather
