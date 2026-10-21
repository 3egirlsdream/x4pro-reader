#include "WeatherJson.h"

#include <cstring>
#include <limits>

namespace weather {
namespace {
constexpr float MISSING = std::numeric_limits<float>::quiet_NaN();
}
bool parseForecast(JsonVariantConst doc, uint32_t cityId, int64_t fetchedAt, Snapshot& out) {
  if (doc["error"] | false) return false;
  const auto units = doc["current_units"];
  if (strcmp(units["temperature_2m"] | "", "°C") || strcmp(units["wind_speed_10m"] | "", "m/s") ||
      strcmp(units["relative_humidity_2m"] | "", "%") || strcmp(units["time"] | "", "unixtime"))
    return false;
  out = {};
  out.cityId = cityId;
  out.fetchedAt = fetchedAt;
  const auto current = doc["current"];
  out.dataTime = current["time"] | int64_t(0);
  out.temperature = current["temperature_2m"] | MISSING;
  out.wind = current["wind_speed_10m"] | MISSING;
  out.humidity = current["relative_humidity_2m"] | int16_t(-1);
  out.code = current["weather_code"] | int16_t(-1);
  const auto daily = doc["daily"];
  const size_t count = daily["time"].size();
  if (count != FORECAST_DAYS || daily["temperature_2m_min"].size() != count ||
      daily["temperature_2m_max"].size() != count || daily["weather_code"].size() != count ||
      daily["precipitation_probability_max"].size() != count)
    return false;
  out.dayCount = count;
  for (size_t i = 0; i < count; ++i) {
    auto& day = out.days[i];
    day.time = daily["time"][i] | int64_t(0);
    day.minimum = daily["temperature_2m_min"][i] | MISSING;
    day.maximum = daily["temperature_2m_max"][i] | MISSING;
    day.code = daily["weather_code"][i] | int16_t(-1);
    day.rainPercent = daily["precipitation_probability_max"][i] | int16_t(-1);
  }
  return validSnapshot(out) && out.dataTime <= fetchedAt + 900 && out.dataTime >= fetchedAt - 86400;
}
void snapshotToJson(const Snapshot& v, JsonDocument& doc) {
  doc["schema"] = 1;
  doc["cityId"] = v.cityId;
  doc["dataTime"] = v.dataTime;
  doc["fetchedAt"] = v.fetchedAt;
  doc["temperature"] = v.temperature;
  doc["wind"] = v.wind;
  doc["humidity"] = v.humidity;
  doc["code"] = v.code;
  auto days = doc["days"].to<JsonArray>();
  for (size_t i = 0; i < v.dayCount; ++i) {
    auto d = days.add<JsonObject>();
    d["time"] = v.days[i].time;
    d["minimum"] = v.days[i].minimum;
    d["maximum"] = v.days[i].maximum;
    d["code"] = v.days[i].code;
    d["rainPercent"] = v.days[i].rainPercent;
  }
}
bool snapshotFromJson(JsonVariantConst doc, Snapshot& out) {
  if ((doc["schema"] | 0) != 1) return false;
  out = {};
  out.cityId = doc["cityId"] | uint32_t(0);
  out.dataTime = doc["dataTime"] | int64_t(0);
  out.fetchedAt = doc["fetchedAt"] | int64_t(0);
  out.temperature = doc["temperature"] | MISSING;
  out.wind = doc["wind"] | MISSING;
  out.humidity = doc["humidity"] | int16_t(-1);
  out.code = doc["code"] | int16_t(-1);
  auto days = doc["days"].as<JsonArrayConst>();
  if (!days.size() || days.size() > FORECAST_DAYS) return false;
  out.dayCount = days.size();
  for (size_t i = 0; i < days.size(); ++i) {
    out.days[i].time = days[i]["time"] | int64_t(0);
    out.days[i].minimum = days[i]["minimum"] | MISSING;
    out.days[i].maximum = days[i]["maximum"] | MISSING;
    out.days[i].code = days[i]["code"] | int16_t(-1);
    out.days[i].rainPercent = days[i]["rainPercent"] | int16_t(-1);
  }
  return validSnapshot(out);
}
}  // namespace weather
