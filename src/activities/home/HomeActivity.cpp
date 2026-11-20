#include "HomeActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include "OpdsServerStore.h"
#include "components/UITheme.h"

void HomeActivity::onEnter() {
  Activity::onEnter();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  shelf.begin();
  ui = makeUniqueNoThrow<CoverGridHomeUi>(renderer);
  if (!ui) {
    LOG_ERR("DESKTOP", "OOM: home UI");
    return;
  }
  ui->begin(shelf, OPDS_STORE.hasServers());
  if (!shelf.visibleCount()) selection = desktop::APP_BASE;
  if (initial == HomeMenuItem::APPS)
    selection = desktop::APP_BASE;
  else if (initial == HomeMenuItem::FILE_BROWSER)
    selection = desktop::APP_BASE + 1;
  else if (initial == HomeMenuItem::FILE_TRANSFER)
    selection = desktop::APP_BASE + 2;
  else if (initial == HomeMenuItem::SETTINGS_MENU)
    selection = desktop::APP_BASE + 3;
  minute = millis() / 60000;
  requestUpdate();
}
void HomeActivity::onExit() {
  shelf.stop();
  ui.reset();
  Activity::onExit();
}
void HomeActivity::changePage(int delta) {
  if (shelf.loading()) return;
  const int target = std::clamp(shelf.currentPage() + delta, 0, desktop::pageCount(shelf.count()) - 1);
  if (target == shelf.currentPage()) return;
  shelf.loadPage(target);
  ui->pageChanged();
  selection = 0;
  requestUpdate();
}
void HomeActivity::activate(int action) {
  const auto start = millis();
  if (action >= 0 && action < shelf.visibleCount() && !shelf.loading()) {
    const auto& path = shelf.visible()[action].path;
    if (Storage.exists(path.c_str()))
      activityManager.goToReader(path, true);
    else {
      shelf.removeMissing();
      ui->pageChanged();
      requestUpdate();
    }
  } else if (action >= desktop::APP_BASE && action < desktop::APP_BASE + ui->appCount()) {
    // Cancel index work before another application can take ownership of storage.
    shelf.stop();
    ui->activateApp(action - desktop::APP_BASE, mappedInput);
  } else if (action == desktop::PAGE_PREVIOUS || action == desktop::PAGE_NEXT) {
    changePage(action == desktop::PAGE_NEXT ? 1 : -1);
  } else if (action == 32) {
    shelf.stop();
    activityManager.goToLibrary();
  }
  LOG_INF("DESKTOP", "dispatch action=%d software_ms=%lu heap=%u max=%u", action,
          static_cast<unsigned long>(millis() - start), unsigned(HalMemory::getInternalHeap().freeBytes),
          unsigned(HalMemory::getInternalHeap().largestBlockBytes));
}
void HomeActivity::processCommand(int command) {
  const int previousSelection = selection;
  if (command >= 0 && command <= 32) {
    activate(command);
    return;
  }
  if (command == 40) {
    if (selection >= desktop::APP_BASE)
      selection = 0;
    else
      changePage(-shelf.currentPage());
  } else if (command == 41 || command == 42) {
    const int dir = command == 42 ? 1 : -1;
    if (selection >= desktop::APP_BASE)
      selection = 0;
    else if (selection + dir >= shelf.visibleCount())
      changePage(1);
    else if (selection + dir < 0)
      changePage(-1);
    else
      selection += dir;
  } else if (command == 43 || command == 44) {
    const int dir = command == 44 ? 1 : -1;
    const int current = selection - desktop::APP_BASE;
    selection = desktop::APP_BASE + (current < 0 ? 0 : (current + ui->appCount() + dir) % ui->appCount());
  }
  if (selection != previousSelection) requestUpdate();
}
void HomeActivity::loop() {
  if (!ui) return;
  // Route every release, even during a paint. Defer its command until the render lock is available.
  int pendingCommand = -1;
  const int action = ui->selectedAction(mappedInput);
  if (action >= 0)
    pendingCommand = action;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm))
    pendingCommand = 45;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Back))
    pendingCommand = 40;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up))
    pendingCommand = 41;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down))
    pendingCommand = 42;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Left))
    pendingCommand = 43;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
    pendingCommand = 44;
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Left)
    pendingCommand = desktop::PAGE_NEXT;
  if (swipe == MappedInputManager::SwipeDir::Down || swipe == MappedInputManager::SwipeDir::Right)
    pendingCommand = desktop::PAGE_PREVIOUS;
  if (pendingCommand >= 0) {
    if (commands.empty()) pendingSince = millis();
    commands.push(pendingCommand);
  }
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (!commands.empty()) {
    LOG_INF("DESKTOP", "input_queue_wait_ms=%lu", static_cast<unsigned long>(millis() - pendingSince));
    const int command = commands.pop();
    processCommand(command == 45 ? selection : command);
    return;
  }
  int x, y;
  if (mappedInput.isScreenTouchHeld(x, y)) return;
  if (firstRenderDone.load() && shelf.step(ui->thumbHeight())) {
    ui->pageChanged();
    requestUpdate();
  }
  const uint32_t now = millis() / 60000;
  if (now != minute) {
    minute = now;
    requestUpdate();
  }
}
void HomeActivity::render(RenderLock&&) {
  const auto start = millis();
  renderer.clearScreen();
  if (ui) {
    ui->setSelection(selection);
    UITheme::drawCoverGridHome(*ui);
  }
  const auto drawMs = millis() - start;
  const auto panelStart = millis();
  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone.load() ? HalDisplay::HALF_REFRESH
                                                                        : HalDisplay::FAST_REFRESH);
  firstRenderDone = true;
  LOG_INF("DESKTOP", "draw_ms=%lu panel_call_ms=%lu", static_cast<unsigned long>(drawMs),
          static_cast<unsigned long>(millis() - panelStart));
}
