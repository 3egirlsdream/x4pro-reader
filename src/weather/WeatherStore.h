#pragma once
#include "WeatherModel.h"

namespace weather {
class Store {
 public:
  Settings settings;
  Snapshot snapshot;
  bool hasCache = false;
  static Store& instance();
  void load();
  bool saveSettings() const;
  bool acceptSnapshot(const Snapshot& value);
  bool saveDiagnostic(const UpdateDiagnostic& value) const;
  bool canEnable() const;
  const City* city() const;
  bool selectCity(uint32_t id);
  bool selectCoordinates(const char* name, float latitude, float longitude);
  bool setEnabled(bool enabled);
  char customName[64]{};
  City customCity{};
  int32_t customLatitudeE5 = 0;
  int32_t customLongitudeE5 = 0;
  void checkBattery(bool userWake);
};
const City* findCity(uint32_t id);
const City* cities();
size_t cityCount();
}  // namespace weather
