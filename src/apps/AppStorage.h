#pragma once
#include <ArduinoJson.h>

namespace appStorage {
bool validId(const char* id);
bool load(const char* id, const char* file, JsonDocument& doc, bool (*validate)(JsonVariantConst) = nullptr);
bool save(const char* id, const char* file, const JsonDocument& doc);
}  // namespace appStorage
