#pragma once
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_STA = 1;
struct FakeWiFi {
  int state = WL_CONNECTED, begins = 0;
  int status() const { return state; }
  void mode(int) {}
  void begin(const char*, const char*) { ++begins; }
};
inline FakeWiFi WiFi;
