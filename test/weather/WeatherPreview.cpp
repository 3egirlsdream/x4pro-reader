#include <GfxRenderer.h>
#include <I18n.h>

#include <fstream>
#include <iostream>

#include "components/UITheme.h"
#include "weather/WeatherJson.h"
#include "weather/WeatherView.h"
int main(int argc, char** argv) {
  if (argc < 4) return 1;
  previewChinese = std::string(argv[2]) == "zh";
  std::ifstream file(argv[1]);
  JsonDocument json;
  if (deserializeJson(json, file) != DeserializationError::Ok) return 2;
  weather::Snapshot snapshot;
  if (!weather::parseForecast(json.as<JsonVariantConst>(), 1816670, json["current"]["time"].as<int64_t>() + 900,
                              snapshot))
    return 3;
  weather::City city{1816670, "北京", previewChinese ? "北京市" : "Beijing", 39.9f, 116.4f};
  const std::string state = argv[3];
  weather::View view{&city,
                     &snapshot,
                     state != "empty",
                     state == "battery",
                     state != "interactive",
                     state == "offline" ? weather::UpdateResult::NetworkError : weather::UpdateResult::Success};
  if (state == "extreme") {
    snapshot.temperature = -100;
    snapshot.wind = 150;
    snapshot.humidity = 100;
    snapshot.code = 99;
    for (auto& day : snapshot.days) day.code = 99;
  }
  if (argc > 4) UITheme::getInstance().metrics.topPadding = std::stoi(argv[4]);
  GfxRenderer renderer;
  weather::drawWeatherView(renderer, view);
  serializeJson(renderer.document, std::cout);
}
