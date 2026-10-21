#include <cassert>

#include "Draw.inc"
#include "Support.h"
int main(int argc, char** argv) {
  const std::string state = argc > 1 ? argv[1] : "normal";
  FixtureShelf shelf;
  const char* titles[] = {"月亮与六便士", "人类简史：从动物到上帝",
                          "山海经",       "长书名测试：在漫长的旅途中寻找世界与自我——未删节完整版",
                          "夜航船",       "边城"};
  for (int i = 0; i < 6; ++i) {
    shelf.books[i].title = titles[i];
    shelf.books[i].path = "/book" + std::to_string(i);
  }
  shelf.books[0].cover = "月亮与六便士";
  shelf.books[2].cover = "山海经";
  shelf.books[0].progress = desktop::Progress::Reading;
  shelf.books[0].percent = 37;
  shelf.books[1].progress = desktop::Progress::Finished;
  shelf.books[1].percent = 100;
  shelf.books[3].progress = desktop::Progress::Unknown;
  if (state == "empty") shelf.size = 0;
  if (state == "loading") {
    shelf.busy = true;
    shelf.size = 0;
  }
  if (state == "large") {
    shelf.size = 2000;
    shelf.page = 162;
    registryCount = 6;
  }
  for (auto& entry : registry) entry.create = &createPreviewApp;
  GfxRenderer renderer;
  JsonTarget target;
  CoverGridHomeUi home{renderer, &shelf};
  if (state == "network") home.opds = true;
  MappedInputManager input;
  for (int i = 0; i < home.appCount(); ++i) {
    destination = -1;
    home.activateApp(i, input);
    assert(destination == (i < 4 ? i : size_t(i - 4) < nativeAppCount() ? 100 : 200));
  }
  destination = -1;
  home.activateApp(99, input);
  assert(destination == -1);
  HomeActivity controller{shelf, &home};
  controller.processCommand(44);
  assert(controller.selection == desktop::APP_BASE);
  controller.processCommand(44);
  assert(controller.selection == desktop::APP_BASE + 1);
  controller.processCommand(40);
  assert(controller.selection == 0);
  controller.processCommand(0);
  assert(controller.lastAction == 0);
  if (shelf.visibleCount() > 1) {
    controller.processCommand(42);
    assert(controller.selection == 1);
    controller.processCommand(41);
    assert(controller.selection == 0);
  }

  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  device.hasTouch = true;
  fui::FreeInkApp<24, 6> app(target, device);
  auto theme = fui::defaultThemeTokens();
  theme.bodyText.font = 1;
  theme.smallText.font = 0;
  theme.titleText.font = 2;
  app.setTheme(theme);
  app.setScreen([](UiScreen& screen, void* p) { static_cast<CoverGridHomeUi*>(p)->draw(screen); }, &home);
  app.render();
  assert(!app.interactionOverflowed());
  const desktop::Layout layout(480, 800, 41, 8, 16);
  // Press -> ordinary repaint -> release still dispatches exactly once.
  for (int i = 0; i < home.appCount(); ++i) {
    const auto box = layout.apps[i];
    fui::InputSnapshot press;
    press.touchPressed = true;
    press.touchX = box.x + box.width / 2;
    press.touchY = box.y + 30;
    assert(!app.route(press));
    app.render();
    fui::InputSnapshot release = press;
    release.touchPressed = false;
    release.touchReleased = true;
    const auto event = app.route(release);
    assert(event && event.value == desktop::APP_BASE + i);
    assert(!app.route({}));
  }
  if (shelf.size && !shelf.busy) {
    for (int i = 0; i < 6; ++i) {
      auto box = layout.books[i];
      fui::InputSnapshot input;
      input.touchReleased = true;
      input.touchX = box.x + 10;
      input.touchY = box.y + 10;
      const auto event = app.route(input);
      assert(event && event.value == i);
    }
  }
  commands.clear();
  app.render();
  serializeJson(commands, std::cout);
}
