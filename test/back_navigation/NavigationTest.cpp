#include <components/controls/header.h>

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

#include "activities/Activity.h"
#include "weather/WeatherTouchLayout.h"

static int checks = 0;
#define CHECK(v)                                                   \
  do {                                                             \
    ++checks;                                                      \
    if (!(v)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #v); \
      std::abort();                                                \
    }                                                              \
  } while (0)
GfxRenderer deviceRenderer;
HalGPIO deviceGpio;
MappedInputManager deviceInput(deviceGpio, deviceRenderer);
ActivityManager activityManager(deviceRenderer, deviceInput);
RenderLock::RenderLock(Mode) { isLocked = true; }
RenderLock::RenderLock(Activity&) : RenderLock() {}
RenderLock::~RenderLock() {}
void RenderLock::unlock() { isLocked = false; }
bool RenderLock::peek() { return false; }
void ActivityManager::requestUpdate(bool) { requestedUpdate = true; }
void ActivityManager::requestUpdateAndWait() { requestedUpdate = true; }
void ActivityManager::renderTaskLoop() { std::abort(); }
void ActivityManager::goToReader(std::string, bool) {}
class FrontlightPanelActivity : public Activity {
 public:
  FrontlightPanelActivity(GfxRenderer& r, MappedInputManager& i) : Activity("FrontlightPanel", r, i) {}
};
void ActivityManager::goHome(HomeMenuItem, bool) {
  replaceActivity(std::make_unique<FrontlightPanelActivity>(renderer, mappedInput));
}
class TestManager : public ActivityManager {
 public:
  using ActivityManager::ActivityManager;
  Activity* current() { return currentActivity.get(); }
  size_t depth() { return stackActivities.size(); }
  bool pending() { return pendingAction != PendingAction::None; }
};
struct Counts {
  int enters = 0, loops = 0, exits = 0, destroyed = 0;
};
class Page : public Activity {
 public:
  Counts& counts;
  TestManager& manager;
  bool exclusive = false, blocked = false;
  Page(const char* name, GfxRenderer& r, MappedInputManager& i, Counts& c, TestManager& m)
      : Activity(name, r, i), counts(c), manager(m) {}
  ~Page() { ++counts.destroyed; }
  void onEnter() override { ++counts.enters; }
  void onExit() override { ++counts.exits; }
  void loop() override {
    ++counts.loops;
    if (!blocked && mappedInput.wasReleased(MappedInputManager::Button::Back)) manager.popActivity();
  }
  bool requiresExclusiveStorageLoop() const override { return exclusive; }
  void handler(std::function<void(const ActivityResult&)> h) { resultHandler = std::move(h); }
};
// Test driver feeds normalized panel points, then uses the production orientation transform.
void point(HalGPIO& gpio, GfxRenderer& r, int x, int y) {
  int px = 0, py = 0;
  switch (r.orientation) {
    case GfxRenderer::Portrait:
      px = y;
      py = 479 - x;
      break;
    case GfxRenderer::PortraitInverted:
      px = 799 - y;
      py = x;
      break;
    case GfxRenderer::LandscapeClockwise:
      px = 799 - x;
      py = 479 - y;
      break;
    case GfxRenderer::LandscapeCounterClockwise:
      px = x;
      py = y;
      break;
  }
  gpio.nx = (px + 0.25f) / 800;
  gpio.ny = (py + 0.25f) / 480;
}
void down(HalGPIO& g, GfxRenderer& r, MappedInputManager& i, int x, int y) {
  g = {};
  g.down = g.held = true;
  point(g, r, x, y);
  i.update();
  g.down = false;
}
void release(HalGPIO& g, MappedInputManager& i, unsigned long ms = 100) {
  g.held = false;
  g.released = g.tap = true;
  g.duration = ms;
  i.update();
}
void idle(HalGPIO& g, MappedInputManager& i) {
  g = {};
  i.update();
}
void publish(HeaderBackTapTarget::Rect rect) {
  HeaderBackTapTarget::beginFrame();
  HeaderBackTapTarget::set(rect);
  HeaderBackTapTarget::publish();
}

int main(int argc, char** argv) {
  FILE* geometry = argc > 1 ? fopen(argv[1], "w") : nullptr;
  if (geometry)
    fprintf(geometry,
            "theme,orientation,scale,screenWidth,screenHeight,bandX,bandY,bandW,bandH,strip,buttonY,targetX,targetY,"
            "targetW,targetH\n");
  GfxRenderer r;
  HalGPIO g;
  MappedInputManager input(g, r);
  // Values generated from the actual four theme headers by the Python harness.
  for (const auto& theme : TEST_THEMES)
    for (int orientation = 0; orientation < 4; ++orientation)
      for (int opticalDrop : {0, 5, 12}) {
        r.orientation = static_cast<GfxRenderer::Orientation>(orientation);
        const int safe = orientation & 1 ? 12 : 0;
        fui::HeaderProps props;
        configureHeader(FontMetricsAdapter{opticalDrop}, HeaderMetrics{theme.height, theme.strip}, props);
        const int offset = props.actionOffsetY;
        static const uint8_t bitmap[128] = {0};
        props.leadingIcon = {bitmap, 32, 32};
        props.leadingAction = 1;
        fui::GfxRendererTarget target(r);
        auto device = target.deviceContext();
        fui::InputSnapshot empty{};
        fui::InteractionBuffer<4> hits;
        fui::Frame<4> frame(target, device, empty, hits);
        fui::header(frame,
                    {static_cast<int16_t>(safe), static_cast<int16_t>(theme.top + safe),
                     static_cast<int16_t>(r.getScreenWidth() - 2 * safe), static_cast<int16_t>(theme.height)},
                    props);
        const auto rect = HeaderBackTapTarget::geometry(safe, theme.top + safe, r.getScreenWidth() - 2 * safe,
                                                        theme.height, theme.strip, 48, offset);
        CHECK(rect.w == 64);
        CHECK(rect.h > 0 && rect.h <= 64);
        CHECK(rect.y >= theme.top + safe + theme.strip);
        CHECK(rect.y + rect.h <= theme.top + safe + theme.height);
        CHECK(rect.contains(safe + 28, theme.top + safe + 4 + offset + 24));
        CHECK(rect.contains(target.lastBitmap.x, target.lastBitmap.y));
        CHECK(rect.contains(target.lastBitmap.x + target.lastBitmap.width - 1,
                            target.lastBitmap.y + target.lastBitmap.height - 1));
        publish(rect);
        auto decoded = HeaderBackTapTarget::decode(HeaderBackTapTarget::snapshot());
        CHECK(decoded.x == rect.x && decoded.y == rect.y && decoded.w == rect.w && decoded.h == rect.h);
        for (auto p : {std::pair{rect.x, rect.y}, std::pair{rect.x + rect.w - 1, rect.y + rect.h - 1},
                       std::pair{rect.x + 32, rect.y + rect.h / 2}}) {
          down(g, r, input, p.first, p.second);
          release(g, input, 1100);
          CHECK(input.wasReleased(MappedInputManager::Button::Back));
          CHECK(input.wasPressed(MappedInputManager::Button::Back));
          CHECK(input.getHeldTime() == 0);
          int x, y;
          CHECK(!input.wasScreenTapped(x, y));
          idle(g, input);
          CHECK(!input.wasReleased(MappedInputManager::Button::Back));
        }
        for (auto p : {std::pair{rect.x - 1, rect.y}, std::pair{rect.x + rect.w, rect.y}, std::pair{rect.x, rect.y - 1},
                       std::pair{rect.x, rect.y + rect.h}}) {
          if (p.first < 0 || p.second < 0) continue;
          down(g, r, input, p.first, p.second);
          release(g, input);
          CHECK(!input.wasBackGesture());
        }
        if (geometry)
          fprintf(geometry, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", theme.name, orientation, opticalDrop,
                  r.getScreenWidth(), r.getScreenHeight(), safe, theme.top + safe, r.getScreenWidth() - 2 * safe,
                  theme.height, theme.strip, theme.top + safe + 4 + offset, rect.x, rect.y, rect.w, rect.h);
      }
  if (geometry) fclose(geometry);
  r.orientation = GfxRenderer::Portrait;
  const HeaderBackTapTarget::Rect rect{0, 35, 64, 54};
  publish(rect);
  // A partial rebuild must not close routing; the complete new frame can hide the target.
  down(g, r, input, 28, 60);
  HeaderBackTapTarget::beginFrame();
  CHECK(HeaderBackTapTarget::contains(28, 60));
  HeaderBackTapTarget::set(rect);
  release(g, input);
  CHECK(input.wasBackGesture());
  HeaderBackTapTarget::publish();
  down(g, r, input, 28, 60);
  HeaderBackTapTarget::beginFrame();
  HeaderBackTapTarget::publish();
  release(g, input);
  CHECK(!input.wasBackGesture());
  // First-frame touch is not retroactively bound to an arrow that appeared after touch-down.
  HeaderBackTapTarget::clear();
  down(g, r, input, 28, 60);
  publish(rect);
  release(g, input);
  CHECK(!input.wasBackGesture());
  int x, y;
  CHECK(!input.wasScreenTapped(x, y));
  // Small roll accepted; an excursion is cancelled even if the finger comes back.
  for (int drift : {0, 10, 28, 29, 59, 100}) {
    publish(rect);
    down(g, r, input, 28, 60);
    point(g, r, 28 + drift, 60);
    input.update();
    point(g, r, 28, 60);
    input.update();
    release(g, input);
    CHECK(input.wasBackGesture() == (drift <= 28));
  }
  down(g, r, input, 28, 60);
  release(g, input, 1200);
  CHECK(!input.wasBackGesture());
  down(g, r, input, 28, 60);
  r.orientation = GfxRenderer::LandscapeCounterClockwise;
  release(g, input);
  CHECK(!input.wasBackGesture());
  r.orientation = GfxRenderer::Portrait;
  down(g, r, input, 28, 60);
  input.suppressNavigationInput();
  release(g, input);
  CHECK(!input.wasBackGesture());
  CHECK(!input.wasScreenTapped(x, y));
  // Footer paint/hit share actual production geometry; the central gap and margins stay inert.
  HeaderBackTapTarget::clear();
  r.orientation = GfxRenderer::LandscapeCounterClockwise;
  auto footer = weather::footerButtonRect(800, 480, 20, false);
  down(g, r, input, footer.x, footer.y);
  release(g, input);
  CHECK(input.wasNavigationTapInRect(footer.x, footer.y, footer.width, footer.height));
  down(g, r, input, 400, 450);
  release(g, input);
  CHECK(!input.wasNavigationTapInRect(footer.x, footer.y, footer.width, footer.height));
  down(g, r, input, 40, 450);
  point(g, r, 100, 450);
  input.update();
  point(g, r, 40, 450);
  release(g, input);
  CHECK(!input.wasNavigationTapInRect(footer.x, footer.y, footer.width, footer.height));
  // Physical Back holds keep their duration; a held key cannot release or long-trigger into the next page.
  idle(g, input);
  g.keys = 1;
  g.pressedKeys = 1;
  g.duration = 1100;
  input.update();
  CHECK(input.wasPressed(MappedInputManager::Button::Back));
  CHECK(input.getHeldTime() == 1100);
  input.suppressNavigationInput();
  g.pressedKeys = 0;
  input.update();
  CHECK(!input.wasLongPressed(MappedInputManager::Button::Back, 1000));
  g.keys = 0;
  g.releasedKeys = 1;
  input.update();
  CHECK(!input.wasReleased(MappedInputManager::Button::Back));
  idle(g, input);
  g.releasedKeys = 1;
  input.update();
  CHECK(input.wasReleased(MappedInputManager::Button::Back));
  idle(g, input);
  // Actual ActivityManager stack methods: parent lifetime, result, one input -> one pop.
  TestManager manager(r, input);
  Counts parent, child, grandchild;
  auto parentPage = std::make_unique<Page>("Parent", r, input, parent, manager);
  auto* parentPtr = parentPage.get();
  manager.replaceActivity(std::move(parentPage));
  CHECK(parent.enters == 1);
  manager.pushActivity(std::make_unique<Page>("Child", r, input, child, manager));
  manager.loop();
  CHECK(manager.depth() == 1);
  CHECK(parent.exits == 0);
  CHECK(child.enters == 1);
  manager.pushActivity(std::make_unique<Page>("Grandchild", r, input, grandchild, manager));
  manager.loop();
  CHECK(manager.depth() == 2);
  publish(rect);
  down(g, r, input, 28, 60);
  release(g, input);
  manager.loop();
  CHECK(manager.depth() == 1);
  CHECK(grandchild.exits == 1 && grandchild.destroyed == 1);
  CHECK(!input.wasBackGesture());
  // Re-entering manager with a consumed frame cannot run another pop.
  manager.loop();
  CHECK(manager.depth() == 1);
  CHECK(child.exits == 0);
  int results = 0;
  parentPtr->handler([&](const ActivityResult&) { ++results; });
  idle(g, input);
  publish(rect);
  down(g, r, input, 28, 60);
  release(g, input);
  manager.loop();
  CHECK(manager.current() == parentPtr);
  CHECK(manager.depth() == 0);
  CHECK(results == 1);
  CHECK(parent.enters == 1);
  CHECK(child.exits == 1);
  // Pending navigation skips the outgoing activity's loop on the next dispatch.
  int loops = parent.loops;
  manager.pushActivity(std::make_unique<Page>("Critical", r, input, child, manager));
  idle(g, input);
  manager.loop();
  CHECK(parent.loops == loops);
  auto* critical = static_cast<Page*>(manager.current());
  critical->exclusive = true;
  critical->blocked = true;
  publish(rect);
  down(g, r, input, 28, 60);
  release(g, input);
  manager.loop();
  CHECK(manager.current() == critical);
  CHECK(manager.depth() == 1);
  manager.popActivity();
  manager.loop();
  CHECK(manager.current() == critical);
  CHECK(manager.pending());
  critical->exclusive = false;
  idle(g, input);
  manager.loop();
  CHECK(manager.current() == parentPtr);
  // Publication is an atomic whole rectangle under concurrent rendering and routing.
  const auto a = HeaderBackTapTarget::encode({0, 35, 64, 54}), b = HeaderBackTapTarget::encode({20, 50, 60, 60});
  HeaderBackTapTarget::published = a;
  std::thread writer([&] {
    for (int n = 0; n < 100000; n++) HeaderBackTapTarget::published.store(n & 1 ? a : b);
  });
  for (int n = 0; n < 100000; n++) {
    auto v = HeaderBackTapTarget::snapshot();
    CHECK(v == a || v == b);
  }
  writer.join();
  printf(
      "Navigation production checks passed: %d; actual mapped input + coordinate transform + activity stack; UBSan "
      "enabled\n",
      checks);
}
