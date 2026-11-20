#pragma once
#include <optional>
#include <string>
struct WifiCredential {
  std::string ssid = "test", password = "test";
};
struct FakeCredentials {
  bool present = true;
  void loadFromFile() {}
  std::string getLastConnectedSsid() const { return "test"; }
  std::optional<WifiCredential> findCredential(const std::string&) const {
    return present ? std::optional<WifiCredential>(WifiCredential{}) : std::nullopt;
  }
};
inline FakeCredentials testCredentials;
#define WIFI_STORE testCredentials
