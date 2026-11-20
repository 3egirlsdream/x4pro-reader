#include <Arduino.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

#include "WifiCredentialStore.h"
#include "weather/WeatherRequest.h"

using namespace weather;
HalPowerManager powerManager;
namespace {
time_t testEpoch = 1792466100;
struct Reply {
  esp_err_t error = ESP_OK;
  int status = 200;
  std::string body{};
  bool pending = false, complete = true;
  int tlsError = 0, tlsCode = 0, tlsFlags = 0;
};
std::vector<Reply> replies;
std::vector<std::string> urls;
int active = 0, maximumActive = 0;
bool failInit = false;
constexpr char DNS[] =
    R"({"Status":0,"Question":[{"name":"api.open-meteo.com.","type":1}],"Answer":[{"type":1,"data":"167.114.211.86"}]})";
const City CITY{1, "", "Test", 39.9042f, 116.4074f};
std::string forecast;
void reset() {
  assert(active == 0);
  testMillis = 0;
  testEpoch = 1792466100;
  testSntp = false;
  testSyncComplete = false;
  halClock = {};
  WiFi = {};
  testCredentials = {};
  replies.clear();
  urls.clear();
  maximumActive = 0;
  failInit = false;
}
UpdateResult run(Request& request) {
  for (int i = 0; i < 12; ++i) {
    const auto result = request.poll();
    if (result != UpdateResult::Running) return result;
  }
  assert(false);
  return UpdateResult::Idle;
}
Reply success(const std::string& body) { return {ESP_OK, 200, body}; }
}  // namespace
extern "C" time_t time(time_t* output) {
  if (output) *output = testEpoch;
  return testEpoch;
}
extern "C" int settimeofday(const timeval* tv, const struct timezone*) {
  testEpoch = tv->tv_sec;
  return 0;
}
struct TestHttp {
  esp_http_client_config_t config;
  Reply reply;
  bool delivered = false;
};
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config) {
  if (failInit) return nullptr;
  assert(config->is_async && config->crt_bundle_attach && !config->skip_cert_common_name_check);
  assert(config->timeout_ms > 0 && config->timeout_ms <= 4000);
  assert(config->disable_auto_redirect);
  assert(!replies.empty());
  std::string url(config->url);
  if (url.find("https://167.114.211.86/") == 0) {
    assert(config->common_name && std::string(config->common_name) == "api.open-meteo.com");
    assert(url.find("latitude=39.90420&longitude=116.40740") != std::string::npos);
  } else {
    assert(!config->common_name);
  }
  urls.push_back(url);
  auto* handle = new (std::nothrow) TestHttp{*config, replies.front()};
  assert(handle);
  replies.erase(replies.begin());
  maximumActive = std::max(maximumActive, ++active);
  return handle;
}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) {
  --active;
  delete h;
  return ESP_OK;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char* key, const char* value) {
  assert(std::string(key) == "Host" && std::string(value) == "api.open-meteo.com");
  return ESP_OK;
}
esp_err_t esp_http_client_perform(esp_http_client_handle_t h) {
  if (h->reply.pending) return ESP_ERR_HTTP_EAGAIN;
  if (!h->delivered && h->reply.error == ESP_OK) {
    h->delivered = true;
    for (size_t offset = 0; offset < h->reply.body.size(); offset += 73) {
      auto event = esp_http_client_event_t{HTTP_EVENT_ON_DATA, h->config.user_data, h->reply.body.data() + offset,
                                           int(std::min(size_t(73), h->reply.body.size() - offset))};
      if (h->config.event_handler(&event) != ESP_OK) return ESP_FAIL;
    }
  }
  return h->reply.error;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) { return h->reply.status; }
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { return h->reply.complete; }
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t h, int* code, int* flags) {
  *code = h->reply.tlsCode;
  *flags = h->reply.tlsFlags;
  return h->reply.tlsError;
}
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream file(argv[1]);
  assert(file.good());
  forecast.assign(std::istreambuf_iterator<char>(file), {});
  Request request;
  reset();
  replies = {success(forecast)};
  request.begin(CITY);
  assert(run(request) == UpdateResult::Success && request.value().dayCount == 5);
  assert(!request.diagnostic().fallback && active == 0 && maximumActive == 1);
  assert(request.poll() == UpdateResult::Success);
  for (bool sync : {true, false}) {
    reset();
    testEpoch -= 3600;
    replies = {success(forecast)};
    request.begin(CITY);
    assert(request.poll() == UpdateResult::Running);
    assert(request.poll() == UpdateResult::Running && active == 0 && testSntp);
    if (sync) {
      testEpoch += 3600;
      testSyncComplete = true;
    }
    testMillis += 6000;
    assert(request.poll() == (sync ? UpdateResult::Success : UpdateResult::ClockError));
    assert(!testSntp && urls.size() == 1);
  }

  reset();
  replies = {{ESP_FAIL}, success(DNS), success(forecast)};
  request.begin(CITY);
  assert(run(request) == UpdateResult::Success && request.diagnostic().fallback);
  assert(urls.size() == 3 && maximumActive == 1 && active == 0);
  assert(urls[1] == "https://dns.google/resolve?name=api.open-meteo.com&type=A");

  reset();
  replies = {{ESP_OK, 200, "", true}, success(DNS), success(forecast)};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  testMillis = 8000;
  assert(request.poll() == UpdateResult::Running);
  assert(run(request) == UpdateResult::Success && urls.size() == 3);

  reset();
  replies = {{ESP_FAIL}, {ESP_FAIL}};
  request.begin(CITY);
  assert(run(request) == UpdateResult::DnsError && active == 0 && urls.size() == 2);
  assert(request.diagnostic().stage == RequestStage::Resolve);

  reset();
  replies = {{ESP_FAIL}, success(DNS), {ESP_FAIL, 0, "", false, true, 300, -123, 4}};
  request.begin(CITY);
  assert(run(request) == UpdateResult::TlsError && active == 0);
  assert(request.diagnostic().tlsFlags == 4 && request.diagnostic().tlsCode == -123);

  reset();
  replies = {{ESP_OK, 429}};
  request.begin(CITY);
  assert(run(request) == UpdateResult::RateLimited && urls.size() == 1);
  assert(request.diagnostic().httpStatus == 429);

  reset();
  replies = {{ESP_OK, 503}};
  request.begin(CITY);
  assert(run(request) == UpdateResult::HttpError && urls.size() == 1);

  reset();
  replies = {success("not JSON")};
  request.begin(CITY);
  assert(run(request) == UpdateResult::InvalidData && active == 0);
  reset();
  replies = {success(std::string(9000, 'x'))};
  request.begin(CITY);
  assert(run(request) == UpdateResult::InvalidData && active == 0);

  for (const char* address : {"127.0.0.1", "192.168.1.1", "10.0.0.1", "169.254.1.2", "100.64.0.1", "224.0.0.1",
                              "1.2.3.4/path", "1.2.3", "256.1.2.3", "01.2.3.4", ""}) {
    reset();
    std::string dns(DNS);
    dns.replace(dns.find("167.114.211.86"), 14, address);
    replies = {{ESP_FAIL}, success(dns)};
    request.begin(CITY);
    assert(run(request) == UpdateResult::DnsError && urls.size() == 2 && active == 0);
  }
  reset();
  replies = {
      {ESP_FAIL},
      success(
          R"({"Status":0,"Question":[{"name":"other.example","type":1}],"Answer":[{"type":1,"data":"167.114.211.86"}]})")};
  request.begin(CITY);
  assert(run(request) == UpdateResult::DnsError && active == 0);

  reset();
  WiFi.state = 0;
  testCredentials.present = false;
  request.begin(CITY);
  assert(request.poll() == UpdateResult::WifiError && urls.empty());
  reset();
  WiFi.state = 0;
  request.begin(CITY);
  testMillis = 8000;
  assert(request.poll() == UpdateResult::WifiError && WiFi.begins == 1);

  reset();
  testEpoch = 0;
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running && testSntp);
  testMillis = 6000;
  assert(request.poll() == UpdateResult::ClockError && !testSntp);

  reset();
  failInit = true;
  request.begin(CITY);
  assert(request.poll() == UpdateResult::NetworkError && active == 0);
  reset();
  replies = {{ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running && active == 1);
  request.cancel();
  assert(request.poll() == UpdateResult::Cancelled && active == 0);
  const auto elapsed = request.diagnostic().elapsedMs;
  request.cancel();
  assert(request.diagnostic().elapsedMs == elapsed);

  reset();
  replies = {{ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  testMillis = 30000;
  assert(request.poll() == UpdateResult::Timeout && active == 0 && urls.size() == 1);

  reset();
  testMillis = UINT32_MAX - 100;
  replies = {{ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  testMillis += 30000;
  assert(request.poll() == UpdateResult::Timeout && active == 0);
  for (int phase = 0; phase < 3; ++phase) {
    reset();
    replies = {{ESP_FAIL}, success(DNS), {ESP_OK, 200, "", true}};
    request.begin(CITY);
    for (int i = 0; i <= phase; ++i) assert(request.poll() == UpdateResult::Running);
    request.cancel();
    assert(active == 0 && request.poll() == UpdateResult::Cancelled);
  }
  reset();
  replies = {{ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  testMillis = 23000;
  assert(request.poll() == UpdateResult::Timeout && urls.size() == 1 && active == 0);
  reset();
  replies = {{ESP_FAIL}, {ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  assert(request.poll() == UpdateResult::Running);
  testMillis = 5000;
  assert(request.poll() == UpdateResult::DnsError && active == 0);
  reset();
  replies = {{ESP_FAIL}, success(DNS), {ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  assert(request.poll() == UpdateResult::Running);
  assert(request.poll() == UpdateResult::Running);
  testMillis = 8000;
  assert(request.poll() == UpdateResult::Timeout && active == 0);
  reset();
  replies = {{ESP_OK, 200, "", true}};
  request.begin(CITY);
  assert(request.poll() == UpdateResult::Running);
  WiFi.state = 0;
  assert(request.poll() == UpdateResult::WifiError && active == 0);
  std::cout << "Production weather request: primary/fallback, TLS identity, deadlines, cancellation, errors and "
               "bounded bodies passed\n";
}
