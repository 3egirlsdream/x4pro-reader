#pragma once
#include <ArduinoJson.h>
#include <FreeInkApp.h>
#include <FreeInkUIIcon.h>

#include <fstream>
#include <iostream>

#include "components/icons/listIcons.h"
#include "components/icons/settings2.h"
#include "desktop/DesktopModel.h"
namespace fui = freeink::ui;
inline constexpr fui::ActionId SELECT = 1;
using UiScreen = fui::FreeInkApp<24, 6>::ScreenType;
inline JsonDocument commands;
inline int viewportWidth = 480, viewportHeight = 800;
inline int fontSize(int font) { return font == 2 ? 22 : font == 1 ? 20 : 16; }
inline JsonObject command(const char* kind, int x, int y, int w = 0, int h = 0) {
  auto o = commands["commands"].add<JsonObject>();
  o["kind"] = kind;
  o["x"] = x;
  o["y"] = y;
  o["w"] = w;
  o["h"] = h;
  return o;
}
struct GfxRenderer {
  int getScreenWidth() const { return viewportWidth; }
  int getScreenHeight() const { return viewportHeight; }
  void drawRect(int x, int y, int w, int h) const { command("outline", x, y, w, h); }
  void fillRect(int x, int y, int w, int h) const {
    if (w > 0 && h > 0) command("fill", x, y, w, h);
  }
  void drawLine(int x, int y, int x2, int y2) const {
    auto o = command("line", x, y);
    o["x2"] = x2;
    o["y2"] = y2;
  }
  void drawIcon(const uint8_t* data, int x, int y, int size) const {
    auto o = command("bitmap", x, y, size, size);
    for (int i = 0; i < size * size / 8; ++i) o["bytes"].add(data[i]);
  }
};
struct JsonTarget : fui::DrawTarget {
  fui::Size measureText(fui::FontId font, const char* s, fui::TextStyle) const override {
    int width = 0;
    for (auto p = (const unsigned char*)s; *p; ++p)
      if ((*p & 0xc0) != 0x80) width += (*p < 128 ? fontSize(font) / 2 : fontSize(font));
    return {int16_t(width), int16_t(fontSize(font) + 4)};
  }
  int16_t lineHeight(fui::FontId font) const override { return fontSize(font) + 4; }
  void fill(fui::Rect r, fui::Paint p, uint8_t = 0, uint8_t = fui::CornersAll) override {
    auto o = command("fill", r.x, r.y, r.width, r.height);
    o["white"] = p.color == fui::Color::White;
  }
  void stroke(fui::Rect r, fui::Paint, uint8_t, uint8_t = 0, uint8_t = fui::CornersAll) override {
    command("outline", r.x, r.y, r.width, r.height);
  }
  void line(fui::Point a, fui::Point b, uint8_t, fui::Paint) override {
    auto o = command("line", a.x, a.y);
    o["x2"] = b.x;
    o["y2"] = b.y;
  }
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect r, const char* s, fui::TextStyle style) override {
    fui::layoutText(*this, r, s, style, [&](const char* line, fui::Rect b) {
      auto o = command("text", b.x, b.y, b.width, b.height);
      o["text"] = line;
      o["size"] = fontSize(style.font);
    });
  }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode, fui::Paint = fui::Paint::solid(fui::Color::Black),
              fui::Rotation = fui::Rotation::None) override {
    if (!b) return;
    auto o = command("bitmap", r.x, r.y, r.width, r.height);
    for (int i = 0; i < ((b.width + 7) / 8) * b.height; ++i) o["bytes"].add(b.data[i]);
  }
};
struct Rect {
  int x, y, width, height;
};
struct Metrics {
  int topPadding = 5, batteryBarHeight = 28, buttonHintsHeight = 0;
};
struct UITheme {
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  Metrics metrics;
  const Metrics& getMetrics() const { return metrics; }
  Rect getScreenSafeArea(const GfxRenderer& r, bool) const { return {0, 0, r.getScreenWidth(), r.getScreenHeight()}; }
};
struct Gui {
  void drawDesktopHeader(const GfxRenderer& r, Rect) const {
    auto o = command("text", 18, 10, 330, 24);
    o["text"] = "无线关闭    14:26";
    o["size"] = 16;
    r.drawRect(r.getScreenWidth() - 62, 12, 38, 15);
    r.fillRect(r.getScreenWidth() - 60, 14, 28, 11);
  }
};
inline Gui GUI;
enum UIIcon {
  None,
  Folder,
  Text,
  Image,
  Book,
  File,
  Recent,
  Settings,
  Transfer,
  Library,
  Wifi,
  Hotspot,
  Bookmark,
  Usb,
  Blocks
};
inline fui::BitmapRef listIconFor(UIIcon i, int) {
  return fui::bitmapFromIcon(i == Folder ? icon_folder_32 : i == Bookmark ? icon_bookmark_32 : icon_blocks_32);
}
class MappedInputManager;
struct Activity;
struct Descriptor {
  const char* title;
  UIIcon icon;
  std::unique_ptr<Activity> (*create)(GfxRenderer&, MappedInputManager&) = nullptr;
};
#include "Registry.inc"
inline size_t nativeAppCount() { return registryCount; }
inline const Descriptor* nativeApps() { return registry; }
struct I18n {
  const char* get(const char* s) const { return s; }
};
inline I18n I18N;
inline const char* translated(const char* key) {
  if (!strcmp(key, "STR_DESKTOP_SHELF")) return "书架 · 最近阅读";
  if (!strcmp(key, "STR_DESKTOP_LOADING")) return "正在整理书库，应用可用";
  if (!strcmp(key, "STR_DESKTOP_ERROR")) return "书库读取失败，请打开文件或书库";
  if (!strcmp(key, "STR_DESKTOP_EMPTY")) return "暂无书籍，请先传入存储卡";
  if (!strcmp(key, "STR_DESKTOP_APPS")) return "常用应用";
  if (!strcmp(key, "STR_DESKTOP_ALL_APPS")) return "全部应用";
  if (!strcmp(key, "STR_DESKTOP_PREVIOUS")) return "上一页";
  if (!strcmp(key, "STR_DESKTOP_NEXT")) return "下一页";
  if (!strcmp(key, "STR_DESKTOP_UNKNOWN")) return "进度未知";
  if (!strcmp(key, "STR_DESKTOP_UNREAD")) return "未读";
  if (!strcmp(key, "STR_DESKTOP_FINISHED")) return "已读完";
  if (!strcmp(key, "STR_BROWSE_FILES")) return "文件";
  if (!strcmp(key, "STR_FILE_TRANSFER")) return "文件传输";
  if (!strcmp(key, "STR_SETTINGS_TITLE")) return "设置";
  return "在线书库";
}
#define tr(id) translated(#id)
struct FixtureBook {
  std::string path, title, cover;
  desktop::Progress progress = desktop::Progress::Unread;
  int percent = -1;
};
struct FixtureShelf {
  std::array<FixtureBook, 6> books;
  int size = 6, page = 0;
  bool busy = false, error = false;
  const auto& visible() const { return books; }
  int visibleCount() const { return std::min(size, 6); }
  int count() const { return size; }
  bool loading() const { return busy; }
  bool failed() const { return error; }
  int currentPage() const { return page; }
};
struct CoverCache {
  void prepare() {}
  bool paint(fui::Rect r, size_t, const std::string& path) {
    if (path.empty()) return false;
    auto o = command("cover", r.x, r.y, r.width, r.height);
    o["text"] = path;
    return true;
  }
};
class CoverGridHomeUi {
 public:
  GfxRenderer& renderer;
  FixtureShelf* shelf;
  CoverCache coverCache;
  bool opds = false;
  int selected = 0, thumbHeightValue = 0;
  int appCount() const { return desktop::homeAppCount(nativeAppCount(), opds); }
  void draw(UiScreen& screen);
  void activateApp(int index, MappedInputManager& input);
};
inline fui::Rect rect(desktop::Box b) { return {int16_t(b.x), int16_t(b.y), int16_t(b.width), int16_t(b.height)}; }
class MappedInputManager {};
struct Activity {
  int destination;
};
inline int destination = -1;
struct ActivityManager {
  void goToFileBrowser() { destination = 1; }
  void goToFileTransfer() { destination = 2; }
  void goToSettings() { destination = 3; }
  void goToBrowser() { destination = 200; }
  void replaceActivity(std::unique_ptr<Activity>&& value) { destination = value->destination; }
};
inline ActivityManager activityManager;
inline void openApps(GfxRenderer&, MappedInputManager&) { destination = 0; }
class HomeActivity {
 public:
  FixtureShelf& shelf;
  CoverGridHomeUi* ui;
  int selection = 0, updates = 0, lastAction = -1;
  void activate(int value) { lastAction = value; }
  void changePage(int delta) {
    shelf.page = std::clamp(shelf.page + delta, 0, desktop::pageCount(shelf.size) - 1);
    selection = 0;
  }
  void requestUpdate() { ++updates; }
  void processCommand(int command);
};

inline std::unique_ptr<Activity> createPreviewApp(GfxRenderer&, MappedInputManager&) {
  return std::make_unique<Activity>(Activity{100});
}
