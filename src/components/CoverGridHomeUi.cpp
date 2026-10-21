#include "CoverGridHomeUi.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

#include "UITheme.h"
#include "UiAppHelpers.h"
#include "activities/ActivityManager.h"
#include "apps/NativeApp.h"
#include "desktop/ReadingHistory.h"
#include "icons/settings2.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId SELECT = 1;
fui::Rect rect(desktop::Box b) { return {int16_t(b.x), int16_t(b.y), int16_t(b.width), int16_t(b.height)}; }
}  // namespace
CoverGridHomeUi::CoverGridHomeUi(GfxRenderer& value) : UiAppHost(value), renderer(value), coverCache(value) {}
void CoverGridHomeUi::begin(const desktop::Shelf& value, bool hasOpds) {
  shelf = &value;
  opds = hasOpds;
  coverCache.begin();
  resetUi();
  app.on(SELECT, &CoverGridHomeUi::onAction, this);
  app.setScreen(&CoverGridHomeUi::screenFn, this);
}
void CoverGridHomeUi::onAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<CoverGridHomeUi*>(user);
  self.pending = event.value;
  self.app.clearTapFlash();
}
int CoverGridHomeUi::selectedAction(const MappedInputManager& input) {
  pending = -1;
  const auto touch = routeTouch(input);
  return touch.snap.touchReleased ? pending : -1;
}
int CoverGridHomeUi::appCount() const { return desktop::homeAppCount(nativeAppCount(), opds); }
void CoverGridHomeUi::activateApp(int index, MappedInputManager& input) {
  if (index < 0 || index >= appCount()) return;
  if (index == 0)
    openApps(renderer, input);
  else if (index == 1)
    activityManager.goToFileBrowser();
  else if (index == 2)
    activityManager.goToFileTransfer();
  else if (index == 3)
    activityManager.goToSettings();
  else if (size_t(index - 4) < nativeAppCount()) {
    auto value = nativeApps()[index - 4].create(renderer, input);
    if (value) activityManager.replaceActivity(std::move(value));
  } else if (opds)
    activityManager.goToBrowser();
}
void CoverGridHomeUi::screenFn(UiScreen& screen, void* user) { static_cast<CoverGridHomeUi*>(user)->draw(screen); }
void CoverGridHomeUi::draw(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  const desktop::Layout layout(
      renderer.getScreenWidth(), renderer.getScreenHeight(), safe.y + metrics.topPadding + metrics.batteryBarHeight + 8,
      renderer.getScreenHeight() - safe.y - safe.height + metrics.buttonHintsHeight + 8, std::max(16, safe.x + 8));
  thumbHeightValue = layout.coverHeight;
  coverCache.prepare();
  auto title = screen.theme().bodyText;
  title.bold = true;
  title.maxLines = 1;
  auto small = screen.theme().smallText;
  small.maxLines = 1;
  auto center = small;
  center.align = fui::TextAlign::Center;
  auto paintText = [&](desktop::Box b, const char* text, const fui::TextStyle& style) {
    screen.target().text(rect(b), text, style);
  };
  paintText(layout.heading, tr(STR_DESKTOP_SHELF), title);
  screen.frame().hit(rect(layout.heading), SELECT, 32, fui::InputTouch);
  if (shelf->loading() || shelf->failed() || shelf->count() == 0) {
    auto message = title;
    message.align = fui::TextAlign::Center;
    message.maxLines = 3;
    paintText(layout.empty,
              shelf->loading()  ? tr(STR_DESKTOP_LOADING)
              : shelf->failed() ? tr(STR_DESKTOP_ERROR)
                                : tr(STR_DESKTOP_EMPTY),
              message);
  } else {
    for (int i = 0; i < shelf->visibleCount(); ++i) {
      const auto& book = shelf->visible()[i];
      const auto box = layout.books[i];
      const fui::Rect cover{int16_t(box.x + 5), int16_t(box.y + 4), int16_t(box.width - 10),
                            int16_t(layout.coverHeight)};
      if (!coverCache.paint(cover, i, book.cover)) {
        screen.target().fill(cover, fui::Paint::solid(fui::Color::White));
        renderer.drawRect(cover.x, cover.y, cover.width, cover.height);
        auto text = title;
        text.align = fui::TextAlign::Center;
        text.maxLines = 4;
        screen.target().text(cover.inset(fui::Insets{12, 10, 8, 10}), book.title.c_str(), text);
      }
      paintText({box.x + 3, box.y + layout.coverHeight + 9, box.width - 6, 23}, book.title.c_str(), center);
      char status[16];
      snprintf(status, sizeof(status), "%d%%", book.percent);
      const char* progressText = book.progress == desktop::Progress::Unread     ? tr(STR_DESKTOP_UNREAD)
                                 : book.progress == desktop::Progress::Unknown  ? tr(STR_DESKTOP_UNKNOWN)
                                 : book.progress == desktop::Progress::Finished ? tr(STR_DESKTOP_FINISHED)
                                                                                : status;
      paintText({box.x + 3, box.y + box.height - 32, box.width - 6, 22}, progressText, center);
      const int barY = box.y + box.height - 8;
      renderer.drawLine(box.x + 5, barY, box.x + box.width - 5, barY);
      if (book.progress == desktop::Progress::Reading || book.progress == desktop::Progress::Finished) {
        const int percent = book.progress == desktop::Progress::Finished ? 100 : book.percent;
        renderer.fillRect(box.x + 5, barY - 1, (box.width - 10) * percent / 100, 3);
      } else if (book.progress == desktop::Progress::Unknown) {
        for (int x = box.x + 5; x < box.x + box.width - 5; x += 8) renderer.fillRect(x, barY - 1, 3, 3);
      }
      if (selected == i) renderer.drawRect(box.x, box.y, box.width, box.height);
      screen.frame().hit(rect(box), SELECT, i, fui::InputTouch);
    }
  }
  // The controls have distinct hit rectangles and stay above the fixed apps band.
  char pages[32];
  snprintf(pages, sizeof(pages), "%d / %d", shelf->currentPage() + 1, desktop::pageCount(shelf->count()));
  paintText({layout.previous.x + layout.previous.width, layout.previous.y + 8,
             layout.next.x - layout.previous.x - layout.previous.width, 28},
            pages, center);
  if (shelf->currentPage() > 0 && !shelf->loading()) {
    paintText(layout.previous, tr(STR_DESKTOP_PREVIOUS), center);
    screen.frame().hit(rect(layout.previous), SELECT, desktop::PAGE_PREVIOUS, fui::InputTouch);
  }
  if (shelf->currentPage() + 1 < desktop::pageCount(shelf->count()) && !shelf->loading()) {
    paintText(layout.next, tr(STR_DESKTOP_NEXT), center);
    screen.frame().hit(rect(layout.next), SELECT, desktop::PAGE_NEXT, fui::InputTouch);
  }
  renderer.drawLine(layout.appHeading.x, layout.appHeading.y - 4, renderer.getScreenWidth() - layout.appHeading.x,
                    layout.appHeading.y - 4);
  paintText(layout.appHeading, tr(STR_DESKTOP_APPS), small);
  for (int i = 0; i < appCount(); ++i) {
    const auto box = layout.apps[i];
    const char* label;
    UIIcon icon;
    if (i == 0) {
      label = tr(STR_DESKTOP_ALL_APPS);
      icon = UIIcon::Blocks;
    } else if (i == 1) {
      label = tr(STR_BROWSE_FILES);
      icon = UIIcon::Folder;
    } else if (i == 2) {
      label = tr(STR_FILE_TRANSFER);
      icon = UIIcon::Transfer;
    } else if (i == 3) {
      label = tr(STR_SETTINGS_TITLE);
      icon = UIIcon::Settings;
    } else if (size_t(i - 4) < nativeAppCount()) {
      label = I18N.get(nativeApps()[i - 4].title);
      icon = nativeApps()[i - 4].icon;
    } else {
      label = tr(STR_OPDS_BROWSER);
      icon = UIIcon::Blocks;
    }
    auto bitmap = listIconFor(icon, 32);
    if (icon == UIIcon::Settings) bitmap = fui::bitmapFromIcon(icon_blocks_32);
    if (icon == UIIcon::Transfer) bitmap = fui::bitmapFromIcon(icon_usb_32);
    if (icon == UIIcon::Settings)
      renderer.drawIcon(Settings2Icon, box.x + (box.width - 32) / 2, box.y + 8, 32);
    else
      screen.target().bitmap({int16_t(box.x + (box.width - 32) / 2), int16_t(box.y + 8), 32, 32}, bitmap,
                             fui::BitmapMode::Center);
    paintText({box.x + 2, box.y + 48, box.width - 4, 28}, label, center);
    if (selected == desktop::APP_BASE + i) renderer.drawRect(box.x, box.y + 2, box.width, box.height - 4);
    screen.frame().hit(rect(box), SELECT, desktop::APP_BASE + i, fui::InputTouch);
  }
  GUI.drawDesktopHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.batteryBarHeight});
}
