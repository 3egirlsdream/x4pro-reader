#pragma once
constexpr int SNTP_OPMODE_POLL = 0;
inline bool testSntp = false;
inline bool testSyncComplete = false;
constexpr int SNTP_SYNC_STATUS_COMPLETED = 1;
inline int sntp_get_sync_status() { return testSyncComplete ? SNTP_SYNC_STATUS_COMPLETED : 0; }
inline bool esp_sntp_enabled() { return testSntp; }
inline void esp_sntp_setoperatingmode(int) {}
inline void esp_sntp_setservername(int, const char*) {}
inline void esp_sntp_init() { testSntp = true; }
inline void esp_sntp_stop() { testSntp = false; }
