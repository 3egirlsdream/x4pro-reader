#include "Performance.h"

#include <HalStorage.h>

#include <cstdio>

namespace performance {
bool saveSnapshot() {
  static constexpr const char* names[] = {"boot_setup",       "font_init",
                                          "font_ensure",      "activity_enter",
                                          "activity_exit",    "navigation_lock_wait",
                                          "render_lock_wait", "reader_frame_inclusive",
                                          "txt_layout",       "txt_persist",
                                          "panel_bw_call",    "panel_gray_call",
                                          "panel_wait",       "shelf_stop_wait",
                                          "shelf_cover",      "weather_poll"};
  static_assert(sizeof(names) / sizeof(names[0]) == static_cast<unsigned>(Stage::Count));
  static constexpr const char* temporary = "/.crosspoint/performance-last.csv.tmp";
  static constexpr const char* destination = "/.crosspoint/performance-last.csv";
  {
    HalFile file;
    if (!Storage.openFileForWrite("PERF", temporary, file)) return false;
    char line[192];
    auto size =
        snprintf(line, sizeof(line),
                 "# %s; uptime_ms=%lu; cumulative since boot; nested stages\nstage,count,total_ms,max_ms,last_ms\n",
                 CROSSPOINT_VERSION, static_cast<unsigned long>(millis()));
    if (size < 0 || size >= int(sizeof(line)) ||
        file.write(reinterpret_cast<uint8_t*>(line), size) != static_cast<size_t>(size))
      return false;
    for (unsigned i = 0; i < static_cast<unsigned>(Stage::Count); ++i) {
      const auto& c = counters[i];
      size = snprintf(line, sizeof(line), "%s,%lu,%lu,%lu,%lu\n", names[i], static_cast<unsigned long>(c.count.load()),
                      static_cast<unsigned long>(c.totalMs.load()), static_cast<unsigned long>(c.maxMs.load()),
                      static_cast<unsigned long>(c.lastMs.load()));
      if (size < 0 || size >= int(sizeof(line)) ||
          file.write(reinterpret_cast<uint8_t*>(line), size) != static_cast<size_t>(size))
        return false;
    }
    file.flush();
  }
  Storage.remove(destination);
  return Storage.rename(temporary, destination);
}
}  // namespace performance
