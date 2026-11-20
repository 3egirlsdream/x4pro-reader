#pragma once
#include <ArduinoJson.h>

#include "WeatherModel.h"

namespace weather {
bool parseDnsAddress(JsonVariantConst doc, const char* hostname, char* address, size_t capacity);
inline bool forecastFresh(int64_t dataTime, int64_t now) { return dataTime <= now + 900 && dataTime >= now - 86400; }
bool parseForecast(JsonVariantConst doc, uint32_t cityId, int64_t fetchedAt, Snapshot& out, bool* stale = nullptr);
void snapshotToJson(const Snapshot& value, JsonDocument& doc);
bool snapshotFromJson(JsonVariantConst doc, Snapshot& out);
}  // namespace weather
