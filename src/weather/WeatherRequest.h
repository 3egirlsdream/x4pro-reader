#pragma once
#include <WiFi.h>
#include <esp_http_client.h>

#include "WeatherModel.h"

namespace weather {
// One heap-owned request holds the response off the Arduino/render task stacks.
// poll() drives IDF's asynchronous HTTPS client; no application-owned worker survives exit.
class Request {
 public:
  ~Request();
  void begin(const City& city);
  UpdateResult poll();
  void cancel();
  const Snapshot& value() const { return snapshot; }

 private:
  enum class Stage { Connecting, Clock, Http, Done };
  Stage stage = Stage::Done;
  UpdateResult result = UpdateResult::Idle;
  uint32_t started = 0;
  uint32_t cityId = 0;
  esp_http_client_handle_t client = nullptr;
  esp_http_client_config_t config{};
  char url[768]{};
  char body[8192]{};
  size_t used = 0;
  bool overflow = false;
  bool ownsSntp = false;
  Snapshot snapshot;
  static esp_err_t onEvent(esp_http_client_event_t* event);
  UpdateResult finish(UpdateResult state);
};
}  // namespace weather
