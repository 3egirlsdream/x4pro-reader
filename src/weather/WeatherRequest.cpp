#include "WeatherRequest.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <Performance.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_sntp.h>
#include <esp_tls_errors.h>
#include <sys/time.h>

#include <algorithm>
#include <cstring>
#include <ctime>

#include "WeatherJson.h"
#include "WifiCredentialStore.h"

namespace weather {
namespace {
constexpr char FORECAST_HOST[] = "api.open-meteo.com";
constexpr char DNS_URL[] = "https://dns.google/resolve?name=api.open-meteo.com&type=A";
constexpr uint32_t CONNECT_MS = 8000;
constexpr uint32_t CLOCK_MS = 6000;
constexpr uint32_t FORECAST_MS = 8000;
constexpr uint32_t RESOLVE_MS = 5000;
constexpr uint32_t MIN_FALLBACK_MS = 8000;
}  // namespace

Request::~Request() { cancel(); }
void Request::closeClient() {
  if (client) {
    esp_http_client_cleanup(client);
    client = nullptr;
  }
}
UpdateResult Request::finish(UpdateResult state) {
  details.stage = stage;
  details.result = state;
  details.elapsedMs = uint32_t(millis() - started);
  if (state != UpdateResult::Success && state != UpdateResult::Cancelled)
    LOG_ERR("WX", "Update failed: stage=%u result=%u err=%ld tls=%ld/%ld flags=%ld http=%ld elapsed=%lu",
            unsigned(stage), unsigned(state), long(details.error), long(details.tlsError), long(details.tlsCode),
            long(details.tlsFlags), long(details.httpStatus), static_cast<unsigned long>(details.elapsedMs));
  closeClient();
  if (ownsSntp) {
    esp_sntp_stop();
    ownsSntp = false;
  }
  stage = RequestStage::Done;
  result = state;
  return result;
}
void Request::cancel() {
  if (stage != RequestStage::Done) finish(UpdateResult::Cancelled);
}
void Request::begin(const City& city) {
  cancel();
  recheckingTime = false;
  snapshot = {};
  details = {};
  alternateAddress[0] = 0;
  cityId = city.id;
  latitude = city.latitude;
  longitude = city.longitude;
  started = stageStarted = millis();
  result = UpdateResult::Running;
  stage = RequestStage::Connecting;
  powerManager.setPowerSaving(false);
  if (WiFi.status() == WL_CONNECTED) return;
  WIFI_STORE.loadFromFile();
  const auto ssid = WIFI_STORE.getLastConnectedSsid();
  const auto credential = WIFI_STORE.findCredential(ssid);
  if (!credential) {
    finish(UpdateResult::WifiError);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(credential->ssid.c_str(), credential->password.empty() ? nullptr : credential->password.c_str());
}
bool Request::startHttp(RequestStage next) {
  closeClient();
  stage = next;
  stageStarted = millis();
  const uint32_t elapsed = uint32_t(stageStarted - started);
  if (elapsed >= UPDATE_BUDGET_MS) return false;
  used = 0;
  overflow = false;
  body[0] = 0;
  details.error = details.tlsError = details.tlsCode = details.tlsFlags = details.httpStatus = 0;
  if (next != RequestStage::Resolve) {
    snprintf(
        url, sizeof(url),
        "https://%s/v1/forecast?latitude=%.5f&longitude=%.5f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
        "&temperature_unit=celsius&wind_speed_unit=ms&timezone=Asia%%2FShanghai&timeformat=unixtime&forecast_days=5",
        next == RequestStage::Alternate ? alternateAddress : FORECAST_HOST, latitude, longitude);
  }
  config = {};
  config.url = next == RequestStage::Resolve ? DNS_URL : url;
  config.timeout_ms = std::min(uint32_t(4000), UPDATE_BUDGET_MS - elapsed);
  config.is_async = true;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  // ESP-TLS uses common_name for both certificate verification and TLS SNI.
  config.common_name = next == RequestStage::Alternate ? FORECAST_HOST : nullptr;
  config.buffer_size = 1024;
  config.buffer_size_tx = 768;
  config.event_handler = onEvent;
  config.user_data = this;
  config.user_agent = "X4ProWeather/0.4.4";
  config.disable_auto_redirect = true;
  client = esp_http_client_init(&config);
  if (!client) {
    LOG_ERR("WX", "Cannot allocate HTTPS client");
    return false;
  }
  if (next == RequestStage::Alternate && esp_http_client_set_header(client, "Host", FORECAST_HOST) != ESP_OK) {
    closeClient();
    return false;
  }
  return true;
}
UpdateResult Request::startFallback() {
  const uint32_t elapsed = uint32_t(millis() - started);
  if (elapsed >= UPDATE_BUDGET_MS || UPDATE_BUDGET_MS - elapsed < MIN_FALLBACK_MS) return finish(UpdateResult::Timeout);
  details.fallback = true;
  LOG_INF("WX", "Trying HTTPS DNS fallback within update budget");
  if (!startHttp(RequestStage::Resolve)) return finish(UpdateResult::NetworkError);
  return UpdateResult::Running;
}
UpdateResult Request::networkFailure(esp_err_t error) {
  details.error = error;
  int code = 0, flags = 0;
  details.tlsError = esp_http_client_get_and_clear_last_tls_error(client, &code, &flags);
  details.tlsCode = code;
  details.tlsFlags = flags;
  LOG_ERR("WX", "HTTPS failure: stage=%u err=%ld tls=%ld/%d flags=%d", unsigned(stage), long(error),
          long(details.tlsError), code, flags);
  if (stage == RequestStage::Primary) return startFallback();
  if (stage == RequestStage::Resolve || details.tlsError == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)
    return finish(UpdateResult::DnsError);
  if (code || flags || (details.tlsError != ESP_OK && details.tlsError != ESP_FAIL))
    return finish(UpdateResult::TlsError);
  return finish(UpdateResult::NetworkError);
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
void Request::startClockSync() {
  if (esp_sntp_enabled()) return;
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "ntp.aliyun.com");
  esp_sntp_setservername(1, "time.cloudflare.com");
  esp_sntp_init();
  ownsSntp = true;
}
UpdateResult Request::poll() {
  performance::Scope timing(performance::Stage::WeatherPoll);
  if (stage == RequestStage::Done) return result;
  if (expired(millis(), started)) return finish(UpdateResult::Timeout);
  const uint32_t stageElapsed = uint32_t(millis() - stageStarted);
  if (stage == RequestStage::Connecting) {
    if (WiFi.status() != WL_CONNECTED) {
      if (stageElapsed >= CONNECT_MS) return finish(UpdateResult::WifiError);
      return UpdateResult::Running;
    }
    if (time(nullptr) < 1577836800) {
      struct tm local{};
      if (halClock.localTime(local)) {
        const time_t epoch = mktime(&local);
        if (epoch >= 1577836800) {
          timeval tv{epoch, 0};
          settimeofday(&tv, nullptr);
        }
      }
      if (time(nullptr) < 1577836800) startClockSync();
    }
    stage = RequestStage::Clock;
    stageStarted = millis();
  }
  if (stage == RequestStage::Clock) {
    if (recheckingTime) {
      const auto now = time(nullptr);
      if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        halClock.setUtc(now);
        if (!forecastFresh(snapshot.dataTime, now)) return finish(UpdateResult::InvalidData);
        snapshot.fetchedAt = now;
        return finish(UpdateResult::Success);
      }
      if (uint32_t(millis() - stageStarted) >= CLOCK_MS) return finish(UpdateResult::ClockError);
      return UpdateResult::Running;
    }
    if (time(nullptr) < 1577836800) {
      if (uint32_t(millis() - stageStarted) >= CLOCK_MS) return finish(UpdateResult::ClockError);
      return UpdateResult::Running;
    }
    if (!startHttp(RequestStage::Primary)) return finish(UpdateResult::NetworkError);
    return UpdateResult::Running;
  }
  if (WiFi.status() != WL_CONNECTED) return finish(UpdateResult::WifiError);
  if (stageElapsed >= (stage == RequestStage::Resolve ? RESOLVE_MS : FORECAST_MS)) {
    if (stage == RequestStage::Primary) return startFallback();
    return finish(stage == RequestStage::Resolve ? UpdateResult::DnsError : UpdateResult::Timeout);
  }
  const esp_err_t err = esp_http_client_perform(client);
  if (expired(millis(), started)) return finish(UpdateResult::Timeout);
  if (overflow) return finish(stage == RequestStage::Resolve ? UpdateResult::DnsError : UpdateResult::InvalidData);
  if (err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_HTTP_CONNECTING) return UpdateResult::Running;
  if (err != ESP_OK) return networkFailure(err);
  details.httpStatus = esp_http_client_get_status_code(client);
  if (details.httpStatus != 200) {
    if (stage == RequestStage::Resolve) return finish(UpdateResult::DnsError);
    return finish(details.httpStatus == 429 ? UpdateResult::RateLimited : UpdateResult::HttpError);
  }
  if (!esp_http_client_is_complete_data_received(client)) return networkFailure(ESP_FAIL);
  JsonDocument doc;
  if (deserializeJson(doc, body, used) != DeserializationError::Ok || doc.overflowed())
    return finish(stage == RequestStage::Resolve ? UpdateResult::DnsError : UpdateResult::InvalidData);
  if (stage == RequestStage::Resolve) {
    if (!parseDnsAddress(doc.as<JsonVariantConst>(), FORECAST_HOST, alternateAddress, sizeof(alternateAddress)))
      return finish(UpdateResult::DnsError);
    // Release DNS JSON and the resolver connection before allocating the next TLS session.
    doc.clear();
    doc.shrinkToFit();
    if (!startHttp(RequestStage::Alternate)) return finish(UpdateResult::NetworkError);
    return UpdateResult::Running;
  }
  bool stale = false;
  if (!parseForecast(doc.as<JsonVariantConst>(), cityId, time(nullptr), snapshot, &stale)) {
    if (!stale) return finish(UpdateResult::InvalidData);
    closeClient();
    startClockSync();
    recheckingTime = true;
    stage = RequestStage::Clock;
    stageStarted = millis();
    return UpdateResult::Running;
  }
  return finish(UpdateResult::Success);
}
}  // namespace weather
