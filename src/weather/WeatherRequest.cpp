#include "WeatherRequest.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <Performance.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include <cstring>
#include <ctime>

#include "WeatherJson.h"
#include "WifiCredentialStore.h"

namespace weather {
Request::~Request() { cancel(); }
UpdateResult Request::finish(UpdateResult state) {
  if (client) {
    esp_http_client_cleanup(client);
    client = nullptr;
  }
  if (ownsSntp) {
    esp_sntp_stop();
    ownsSntp = false;
  }
  stage = Stage::Done;
  result = state;
  return result;
}
void Request::cancel() { finish(UpdateResult::Cancelled); }
void Request::begin(const City& city) {
  cancel();
  used = 0;
  overflow = false;
  body[0] = 0;
  snapshot = {};
  cityId = city.id;
  started = millis();
  result = UpdateResult::Running;
  stage = Stage::Connecting;
  powerManager.setPowerSaving(false);
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&temperature_unit=celsius&wind_speed_unit=ms&timezone=Asia%%2FShanghai&timeformat=unixtime&forecast_days=5",
           city.latitude, city.longitude);
  if (WiFi.status() == WL_CONNECTED) return;
  WIFI_STORE.loadFromFile();
  auto ssid = WIFI_STORE.getLastConnectedSsid();
  auto credential = WIFI_STORE.findCredential(ssid);
  if (!credential) {
    finish(UpdateResult::NetworkError);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(credential->ssid.c_str(), credential->password.empty() ? nullptr : credential->password.c_str());
}
esp_err_t Request::onEvent(esp_http_client_event_t* event) {
  auto* self = static_cast<Request*>(event->user_data);
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) return ESP_OK;
  if (size_t(event->data_len) > sizeof(self->body) - 1 - self->used) {
    self->overflow = true;
    return ESP_FAIL;
  }
  memcpy(self->body + self->used, event->data, event->data_len);
  self->used += event->data_len;
  self->body[self->used] = 0;
  return ESP_OK;
}
UpdateResult Request::poll() {
  performance::Scope timing(performance::Stage::WeatherPoll);
  if (stage == Stage::Done) return result;
  if (expired(millis(), started)) return finish(UpdateResult::Timeout);
  if (stage == Stage::Connecting) {
    if (WiFi.status() != WL_CONNECTED) return UpdateResult::Running;
    // Recover the system epoch from the hardware RTC before certificate validation.
    if (time(nullptr) < 1577836800) {
      struct tm local{};
      if (halClock.localTime(local)) {
        const time_t epoch = mktime(&local);
        if (epoch >= 1577836800) {
          timeval tv{epoch, 0};
          settimeofday(&tv, nullptr);
        }
      }
      if (time(nullptr) < 1577836800 && !esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_setservername(1, "time.cloudflare.com");
        esp_sntp_init();
        ownsSntp = true;
      }
    }
    stage = Stage::Clock;
  }
  if (stage == Stage::Clock) {
    if (time(nullptr) < 1577836800) return UpdateResult::Running;
    config = {};
    config.url = url;
    config.timeout_ms = 1000;
    config.is_async = true;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    config.buffer_size_tx = 768;
    config.event_handler = onEvent;
    config.user_data = this;
    config.disable_auto_redirect = true;
    client = esp_http_client_init(&config);
    if (!client) return finish(UpdateResult::NetworkError);
    esp_http_client_set_header(client, "User-Agent", "X4ProWeather/0.2");
    stage = Stage::Http;
  }
  const esp_err_t err = esp_http_client_perform(client);
  if (expired(millis(), started)) return finish(UpdateResult::Timeout);
  if (err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_HTTP_CONNECTING) return UpdateResult::Running;
  if (err != ESP_OK || overflow || esp_http_client_get_status_code(client) != 200 ||
      !esp_http_client_is_complete_data_received(client))
    return finish(UpdateResult::NetworkError);
  JsonDocument doc;
  if (deserializeJson(doc, body, used) != DeserializationError::Ok || doc.overflowed() ||
      !parseForecast(doc.as<JsonVariantConst>(), cityId, time(nullptr), snapshot))
    return finish(UpdateResult::InvalidData);
  return finish(UpdateResult::Success);
}
}  // namespace weather
