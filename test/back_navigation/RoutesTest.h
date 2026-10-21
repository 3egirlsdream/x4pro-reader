#include "components/UiAppHost.h"
inline freeink::ui::GfxRendererTarget makeUiTarget(const GfxRenderer& r) { return freeink::ui::GfxRendererTarget(r); }
template <class T>
void applySharedUiTheme(T&, const freeink::ui::GfxRendererTarget&) {}

class OptionPopup {
 public:
  static constexpr int MAX_OPTIONS = 16;
  static constexpr fui::ActionId ACTION_OPTION = 1, ACTION_CHROME = 2;
  bool active = true;
  int selectedIndex = 0;
  std::vector<std::string> ownedStrings{"Cancel", "Confirm"};
  std::function<void(int)> onSelectCallback;
  std::atomic<bool> uiReady{false};
  fui::InteractionBuffer<17> interactions;
  bool handleInput(MappedInputManager&, const std::function<void()>&);
};
class KeyboardEntryActivity {
 public:
  MappedInputManager& mappedInput;
  int cancelled = 0, fallback = 0;
  explicit KeyboardEntryActivity(MappedInputManager& i) : mappedInput(i) {}
  void onCancel() { ++cancelled; }
  void loop();
};
enum class WifiSelectionState { CONNECTING, AUTO_CONNECTING };
struct WifiAdapter {
  int disconnects = 0;
  void disconnect() { ++disconnects; }
};
inline WifiAdapter WiFi;
class WifiSelectionActivity {
 public:
  MappedInputManager& mappedInput;
  WifiSelectionState state = WifiSelectionState::CONNECTING;
  int completed = 0, checked = 0, lists = 0;
  explicit WifiSelectionActivity(MappedInputManager& i) : mappedInput(i) {}
  void onComplete(bool ok) {
    CHECK(!ok);
    ++completed;
  }
  void checkConnectionStatus() { ++checked; }
  void showNetworkListFromAutoConnect() { ++lists; }
  void loop();
};
class ClearCacheActivity {
 public:
  enum { WARNING, CLEARING, SUCCESS, FAILED };
  int state = WARNING, exits = 0, updates = 0;
  MappedInputManager& mappedInput;
  OptionPopup confirmPopup;
  explicit ClearCacheActivity(MappedInputManager& i) : mappedInput(i) {}
  void requestUpdate() { ++updates; }
  void goBack() { ++exits; }
  void loop();
};
struct TestNav {
  int selected = 2, top = 0;
};
class LibraryListActivity {
 public:
  MappedInputManager& mappedInput;
  TestNav nav;
  std::string query;
  bool groupsCollapsed = false, degraded = false;
  int homes = 0, updates = 0;
  explicit LibraryListActivity(MappedInputManager& input) : mappedInput(input) {}
  TestNav& activeNav() { return nav; }
  bool tabsFocused() { return nav.selected == 0; }
  void applyFilter() {}
  void restoreExpandedList() { groupsCollapsed = false; }
  void requestUpdate() { ++updates; }
  void onGoHome() { ++homes; }
  void handleBackAction();
};
class SettingsActivity {
 public:
  MappedInputManager& mappedInput;
  TestNav nav;
  int homes = 0, updates = 0;
  explicit SettingsActivity(MappedInputManager& input) : mappedInput(input) {}
  int ringPos() { return nav.selected; }
  TestNav& activeNav() { return nav; }
  void stepTab(int) {}
  void toggleCurrentSetting() {}
  void requestUpdate() { ++updates; }
  void onGoHome() { ++homes; }
  bool handleButtons();
};
enum class UsbDriveState { WaitingForHost, Connected, IoError, Ejected, Disconnected, Unsupported };
struct StorageAdapter {
  UsbDriveState state = UsbDriveState::Connected;
  UsbDriveState usbDriveState() { return state; }
  bool usbDriveHostSuspended() { return false; }
  bool disconnectUsbDriveHost() { return true; }
};
inline StorageAdapter Storage;
class UsbDriveActivity {
 public:
  using State = UsbDriveState;
  MappedInputManager& mappedInput;
  State state = State::Connected;
  int restarts = 0;
  bool startFailed = false, hostSuspendPending = false, forcedDisconnectRequested = false;
  unsigned long hostSuspendStartedAt = 0, hostWaitStartedAt = 0, startFailureStartedAt = 0,
                forcedDisconnectRequestedAt = 0;
  static constexpr unsigned long HOST_SUSPEND_TIMEOUT_MS = 2000, HOST_WAIT_TIMEOUT_MS = 300000,
                                 START_FAILURE_TIMEOUT_MS = 30000, FORCED_DISCONNECT_TIMEOUT_MS = 1000;
  explicit UsbDriveActivity(MappedInputManager& input) : mappedInput(input) {}
  void requestUpdate() {}
  void restartToHome() { ++restarts; }
  void loop();
};
class SdFirmwareUpdateActivity {
 public:
  enum class State { UPDATING, FAILED, PICKING };
  State state = State::UPDATING;
  MappedInputManager& mappedInput;
  bool recoveryMode = false;
  int exits = 0, pickers = 0;
  explicit SdFirmwareUpdateActivity(MappedInputManager& input) : mappedInput(input) {}
  void finish() { ++exits; }
  void launchPicker() { ++pickers; }
  void loop();
};
void checkRoutes(GfxRenderer& r, HalGPIO& g, MappedInputManager& input) {
  r.orientation = GfxRenderer::Portrait;
  publish({0, 35, 64, 54});
  down(g, r, input, 28, 60);
  release(g, input);
  KeyboardEntryActivity keyboard(input);
  keyboard.loop();
  CHECK(keyboard.cancelled == 1);
  CHECK(keyboard.fallback == 0);
  SettingsActivity settings(input);
  CHECK(settings.handleButtons());
  CHECK(settings.homes == 1);
  LibraryListActivity library(input);
  library.handleBackAction();
  CHECK(library.homes == 1);
  library.homes = 0;
  library.query = "search";
  library.handleBackAction();
  CHECK(library.query.empty());
  CHECK(library.homes == 0);
  library.groupsCollapsed = true;
  library.handleBackAction();
  CHECK(!library.groupsCollapsed);
  CHECK(library.homes == 0);
  UsbDriveActivity usb(input);
  usb.loop();
  CHECK(usb.restarts == 0);
  Storage.state = UsbDriveState::WaitingForHost;
  usb.loop();
  CHECK(usb.restarts == 1);
  SdFirmwareUpdateActivity flash(input);
  flash.loop();
  CHECK(flash.exits == 0);
  flash.state = SdFirmwareUpdateActivity::State::FAILED;
  flash.loop();
  CHECK(flash.exits == 1);
  flash.recoveryMode = true;
  flash.loop();
  CHECK(flash.pickers == 1);
  CHECK(flash.exits == 1);
  for (auto state : {WifiSelectionState::CONNECTING, WifiSelectionState::AUTO_CONNECTING}) {
    WifiSelectionActivity wifi(input);
    wifi.state = state;
    wifi.loop();
    CHECK(wifi.completed == 1);
    CHECK(wifi.checked == 0);
  }
  OptionPopup popup;
  int repaints = 0;
  popup.handleInput(input, [&] { ++repaints; });
  CHECK(!popup.active);
  CHECK(repaints == 1);
  // An edge swipe produces a raw off-target release. Back must precede that generic touch branch.
  HeaderBackTapTarget::clear();
  idle(g, input);
  point(g, r, 2, 350);
  g.swipe = g.released = true;
  HalGPIO end;
  point(end, r, 140, 350);
  g.endX = end.nx;
  g.endY = end.ny;
  input.update();
  popup.active = true;
  popup.handleInput(input, [&] { ++repaints; });
  CHECK(!popup.active);
  // Cache-warning dismissal completes on the following loop instead of leaving an inert warning page.
  idle(g, input);
  ClearCacheActivity cache(input);
  cache.confirmPopup.active = false;
  cache.loop();
  CHECK(cache.exits == 1);
  // Actual UiAppHost + SDK InteractionBuffer, with a display adapter only.
  UiAppHost host(r);
  host.resetUi();
  int actions = 0;
  host.app.on(1, [](const fui::ActionEvent&, void* user) { ++*static_cast<int*>(user); }, &actions);
  host.app.setScreen(
      [](UiAppHost::UiScreen& screen, void*) { screen.frame().hit({20, 200, 100, 64}, 1, 0, fui::InputTouch); },
      nullptr);
  down(g, r, input, 40, 220);
  release(g, input);
  CHECK(!host.routeTouch(input));
  CHECK(actions == 0);
  host.renderUi();
  down(g, r, input, 40, 220);
  g.duration = 150;
  input.update();
  host.routeTouch(input);
  host.renderUi();
  release(g, input, 700);
  CHECK(host.routeTouch(input));
  CHECK(actions == 1);
  host.closeRouting();
  down(g, r, input, 40, 220);
  release(g, input);
  CHECK(!host.routeTouch(input));
  CHECK(actions == 1);
  host.renderUi();
  down(g, r, input, 40, 220);
  input.suppressNavigationInput();
  release(g, input);
  CHECK(!host.routeTouch(input));
  CHECK(actions == 1);
}
