#pragma once
#include "WeatherModel.h"
class GfxRenderer;
namespace weather {
struct View {
  const City* city;
  const Snapshot* snapshot;
  bool hasCache;
  bool batteryPaused;
  bool unattended;
  UpdateResult result;
};
void drawWeatherView(GfxRenderer& renderer, const View& view);
}  // namespace weather
