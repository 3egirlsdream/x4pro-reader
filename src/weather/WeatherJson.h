#pragma once
#include <ArduinoJson.h>

#include "WeatherModel.h"

namespace weather {
bool parseForecast(JsonVariantConst doc, uint32_t cityId, int64_t fetchedAt, Snapshot& out);
void snapshotToJson(const Snapshot& value, JsonDocument& doc);
bool snapshotFromJson(JsonVariantConst doc, Snapshot& out);
}  // namespace weather
