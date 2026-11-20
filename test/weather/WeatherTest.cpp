#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>

#include "weather/WeatherJson.h"
#include "weather/WeatherStore.h"

using namespace weather;
namespace {
Snapshot sample() {
  Snapshot s;
  s.cityId = cities()[0].id;
  s.fetchedAt = 1792454400;
  s.dataTime = s.fetchedAt;
  s.temperature = 20.5;
  s.wind = 2.1;
  s.humidity = 70;
  s.code = 3;
  s.dayCount = FORECAST_DAYS;
  for (size_t i = 0; i < FORECAST_DAYS; ++i) s.days[i] = {s.dataTime + int64_t(i) * 86400, 10, 25, 3, 50};
  return s;
}
void forecast(JsonDocument& d, const Snapshot& s) {
  d["current_units"]["temperature_2m"] = "°C";
  d["current_units"]["wind_speed_10m"] = "m/s";
  d["current_units"]["relative_humidity_2m"] = "%";
  d["current_units"]["time"] = "unixtime";
  d["current"]["time"] = s.dataTime;
  d["current"]["temperature_2m"] = s.temperature;
  d["current"]["relative_humidity_2m"] = s.humidity;
  d["current"]["wind_speed_10m"] = s.wind;
  d["current"]["weather_code"] = s.code;
  for (const auto& day : s.days) {
    d["daily"]["time"].add(day.time);
    d["daily"]["temperature_2m_min"].add(day.minimum);
    d["daily"]["temperature_2m_max"].add(day.maximum);
    d["daily"]["weather_code"].add(day.code);
    d["daily"]["precipitation_probability_max"].add(day.rainPercent);
  }
}
}  // namespace
int main(int argc, char** argv) {
  FrameGate frames;
  assert(frames.claim(UpdateResult::Running, false));
  for (int i = 0; i < 1000; ++i) assert(!frames.claim(UpdateResult::Running, false));
  assert(frames.claim(UpdateResult::Success, false));
  assert(!frames.claim(UpdateResult::Success, false));
  assert(frames.claim(UpdateResult::Running, false));
  assert(frames.claim(UpdateResult::Timeout, false));
  assert(frames.claim(UpdateResult::Timeout, true));
  FrameGate fastCompletion;
  assert(fastCompletion.claim(UpdateResult::Success, false, 1));
  assert(fastCompletion.claim(UpdateResult::Success, false, 2));
  for (int i = 0; i < 1000; ++i) assert(!fastCompletion.claim(UpdateResult::Success, false, 2));
  float coordinate = 0;
  assert(parseCoordinate(" 31.23040 ", -90, 90, coordinate));
  assert(!parseCoordinate("31.2,121.5", -90, 90, coordinate));
  assert(!parseCoordinate("nan", -90, 90, coordinate));
  assert(!parseCoordinate("inf", -90, 90, coordinate));
  assert(!parseCoordinate("", -90, 90, coordinate));
  assert(!parseCoordinate("91", -90, 90, coordinate));
  assert(validCoordinates(-90, 180) && !validCoordinates(0, 181));
  assert(coordinateLocationId(31.2304f, 121.4737f) & 0x80000000U);
  assert(coordinateLocationId(31.2304f, 121.4737f) != coordinateLocationId(31.2314f, 121.4737f));
  assert(coordinateLocationId(31.2304f, 121.4737f) == coordinateLocationId(31.2304f, 121.4737f));
  assert(coordinateLocationId(999, 0) == 0);

  const auto s = sample();
  assert(validSnapshot(s));
  Snapshot out;
  JsonDocument d;
  snapshotToJson(s, d);
  assert(snapshotFromJson(d.as<JsonVariantConst>(), out));
  assert(out.dayCount == 5 && out.temperature == s.temperature && out.days[4].time == s.days[4].time);
  d["temperature"] = nullptr;
  assert(!snapshotFromJson(d.as<JsonVariantConst>(), out));
  snapshotToJson(s, d);
  d["schema"] = 2;
  assert(!snapshotFromJson(d.as<JsonVariantConst>(), out));
  d.clear();
  forecast(d, s);
  assert(parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d["current"]["temperature_2m"] = nullptr;
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d.clear();
  forecast(d, s);
  d["current_units"]["wind_speed_10m"] = "km/h";
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d.clear();
  forecast(d, s);
  d["daily"]["temperature_2m_min"].as<JsonArray>().remove(4);
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d.clear();
  forecast(d, s);
  d["daily"]["weather_code"][0] = 123;
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d.clear();
  forecast(d, s);
  d["error"] = true;
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt, out));
  d.clear();
  forecast(d, s);
  assert(!parseForecast(d.as<JsonVariantConst>(), s.cityId, s.fetchedAt + 90000, out));
  auto bad = s;
  bad.humidity = 101;
  assert(!validSnapshot(bad));
  bad = s;
  bad.wind = std::numeric_limits<float>::infinity();
  assert(!validSnapshot(bad));
  bad = s;
  bad.days[1].time = bad.days[0].time;
  assert(!validSnapshot(bad));
  bad = s;
  bad.days[0].minimum = 30;
  assert(!validSnapshot(bad));
  Settings settings;
  settings.cityId = s.cityId;
  assert(fresh(s, settings, s.fetchedAt + 1799));
  assert(!fresh(s, settings, s.fetchedAt + 1800));
  assert(!fresh(s, settings, s.fetchedAt - 1));
  settings.cityId++;
  assert(!fresh(s, settings, s.fetchedAt));
  assert(validInterval(15) && validInterval(30) && validInterval(60) && !validInterval(0) && !validInterval(31));
  assert(!pauseForBattery(false, true, 16, false));
  assert(pauseForBattery(false, true, 15, false));
  assert(pauseForBattery(false, false, 80, true));
  assert(pauseForBattery(true, true, 19, true));
  assert(pauseForBattery(true, true, 20, false));
  assert(!pauseForBattery(true, true, 20, true));
  assert(pauseForBattery(false, true, 101, true));
  assert(!expired(100, 0) && expired(30000, 0));
  assert(!expired(100, UINT32_MAX - 10));
  assert(expired(30000, UINT32_MAX - 10));
  assert(cityCount() >= 300);
  for (size_t i = 0; i < cityCount(); ++i) {
    assert(findCity(cities()[i].id) == &cities()[i]);
    assert(cities()[i].latitude >= -90 && cities()[i].latitude <= 90);
    for (size_t j = 0; j < i; ++j) assert(cities()[i].id != cities()[j].id);
  }
  if (argc > 1) {
    std::ifstream file(argv[1]);
    assert(file.good());
    d.clear();
    assert(deserializeJson(d, file) == DeserializationError::Ok);
    assert(parseForecast(d.as<JsonVariantConst>(), s.cityId, d["current"]["time"].as<int64_t>() + 900, out));
  }
  std::cout << "Weather model, parser, cache identity, city catalogue and battery/deadline policies passed\n";
}
