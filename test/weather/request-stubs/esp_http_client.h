#pragma once
#include <cstddef>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_HTTP_EAGAIN = 1001, ESP_ERR_HTTP_CONNECTING = 1002;
constexpr int HTTP_EVENT_ON_DATA = 1;
struct esp_http_client_event_t {
  int event_id;
  void* user_data;
  void* data;
  int data_len;
};
using esp_http_client_handle_t = struct TestHttp*;
struct esp_http_client_config_t {
  const char* url = nullptr;
  int timeout_ms = 0;
  bool is_async = false;
  esp_err_t (*crt_bundle_attach)(void*) = nullptr;
  const char* common_name = nullptr;
  bool skip_cert_common_name_check = false;
  int buffer_size = 0, buffer_size_tx = 0;
  esp_err_t (*event_handler)(esp_http_client_event_t*) = nullptr;
  void* user_data = nullptr;
  const char* user_agent = nullptr;
  bool disable_auto_redirect = false;
};
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char*, const char*);
esp_err_t esp_http_client_perform(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t, int*, int*);
