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
  const UpdateDiagnostic& diagnostic() const { return details; }

 private:
  RequestStage stage = RequestStage::Done;
  UpdateResult result = UpdateResult::Idle;
  uint32_t started = 0;
  uint32_t stageStarted = 0;
  uint32_t cityId = 0;
  float latitude = 0, longitude = 0;
  UpdateDiagnostic details;
  esp_http_client_handle_t client = nullptr;
  esp_http_client_config_t config{};
  char url[768]{};
  char body[8192]{};
  char alternateAddress[16]{};
  size_t used = 0;
  bool overflow = false;
  void startClockSync();
  bool recheckingTime = false;
  bool ownsSntp = false;
  Snapshot snapshot;
  static esp_err_t onEvent(esp_http_client_event_t* event);
  UpdateResult finish(UpdateResult state);
  bool startHttp(RequestStage next);
  UpdateResult startFallback();
  UpdateResult networkFailure(esp_err_t error);
  void closeClient();
};
}  // namespace weather
