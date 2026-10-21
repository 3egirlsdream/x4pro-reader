#include "WeatherView.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cmath>
#include <ctime>

#include "WeatherGraphics.h"
#include "WeatherTouchLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace weather {
namespace {
graphics::WxCategory category(int code) {
  if (code == 0) return graphics::WxCategory::Clear;
  if (code <= 2) return graphics::WxCategory::PartlyCloudy;
  if (code == 3) return graphics::WxCategory::Cloudy;
  if (code == 45 || code == 48) return graphics::WxCategory::Fog;
  if (code >= 51 && code <= 57) return graphics::WxCategory::Drizzle;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return graphics::WxCategory::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return graphics::WxCategory::Snow;
  return graphics::WxCategory::Storm;
}
const char* condition(int code) {
  switch (category(code)) {
    case graphics::WxCategory::Clear:
      return tr(STR_WX_CLEAR);
    case graphics::WxCategory::PartlyCloudy:
      return tr(STR_WX_PARTLY_CLOUDY);
    case graphics::WxCategory::Cloudy:
      return tr(STR_WX_CLOUDY);
    case graphics::WxCategory::Fog:
      return tr(STR_WX_FOG);
    case graphics::WxCategory::Drizzle:
      return tr(STR_WX_DRIZZLE);
    case graphics::WxCategory::Rain:
      return tr(STR_WX_RAIN);
    case graphics::WxCategory::Snow:
      return tr(STR_WX_SNOW);
    default:
      return tr(STR_WX_STORM);
  }
}
void stamp(char* output, size_t size, int64_t epoch, const char* format) {
  const time_t beijing = epoch + 8 * 3600;
  struct tm local{};
  gmtime_r(&beijing, &local);
  strftime(output, size, format, &local);
}
}  // namespace
void drawWeatherView(GfxRenderer& renderer, const View& view) {
  renderer.clearScreen();
  const auto& v = *view.snapshot;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int m = std::max(16, metrics.contentSidePadding);
  const int top = metrics.topPadding + metrics.headerHeight + 8;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, w, metrics.headerHeight},
                 view.city ? view.city->name : tr(STR_APP_WEATHER), nullptr, false);
  char text[96];
  char date[32];
  if (!view.hasCache) {
    UITheme::drawCenteredText(renderer, Rect{0, 0, w, h}, UI_12_FONT_ID, h / 2, tr(STR_WX_NO_DATA));
  } else {
    snprintf(text, sizeof(text), "%.1f", v.temperature);
    graphics::drawBigText(renderer, m, top + 15, text, 10);
    renderer.drawText(UI_12_FONT_ID, m + graphics::bigTextWidth(text, 10) + 10, top + 58, tr(STR_WX_CELSIUS));
    graphics::drawWeatherIcon(renderer, category(v.code), w / 2 + 5, top + 8, 80);
    renderer.drawText(UI_12_FONT_ID, w / 2 + 125, top + 30, condition(v.code));
    snprintf(text, sizeof(text), tr(STR_WX_HUMIDITY_VALUE), v.humidity);
    renderer.drawText(UI_10_FONT_ID, m, top + 95, text);
    snprintf(text, sizeof(text), tr(STR_WX_WIND_VALUE), v.wind);
    renderer.drawText(UI_10_FONT_ID, w / 2, top + 95, text);
    const int dayY = top + 135;
    const int col = (w - m * 2) / FORECAST_DAYS;
    renderer.drawLine(m, dayY - 12, w - m, dayY - 12);
    for (size_t i = 0; i < v.dayCount; ++i) {
      const int x = m + i * col;
      stamp(date, sizeof(date), v.days[i].time, "%m/%d");
      renderer.drawText(UI_10_FONT_ID, x + 8, dayY, date);
      graphics::drawWeatherIcon(renderer, category(v.days[i].code), x + 10, dayY + 30, 45);
      snprintf(text, sizeof(text), "%.0f / %.0f", v.days[i].maximum, v.days[i].minimum);
      renderer.drawText(UI_10_FONT_ID, x + 8, dayY + 80, text);
      const char* label = condition(v.days[i].code);
      const int labelFont = renderer.getTextWidth(UI_10_FONT_ID, label) > col - 32 ? SMALL_FONT_ID : UI_10_FONT_ID;
      renderer.drawText(labelFont, x + 8, dayY + 106, label);
    }
  }
  const int footer = h - (view.unattended ? 64 : 104);
  renderer.drawLine(m, footer - 8, w - m, footer - 8);
  if (view.hasCache) {
    stamp(date, sizeof(date), v.fetchedAt, "%m-%d %H:%M");
    snprintf(text, sizeof(text), tr(STR_WX_UPDATED_VALUE), date);
    renderer.drawText(UI_10_FONT_ID, m, footer, text);
  }
  const char* status = view.batteryPaused                     ? tr(STR_WX_BATTERY_PAUSED)
                       : view.result == UpdateResult::Running ? tr(STR_WX_UPDATING)
                       : view.result != UpdateResult::Idle && view.result != UpdateResult::Success ? tr(STR_WX_FAILED)
                       : view.result == UpdateResult::Success                                      ? tr(STR_WX_COMPLETE)
                                                              : tr(STR_WX_MODEL_DATA);
  renderer.drawText(UI_10_FONT_ID, w / 2, footer, status);
  renderer.drawText(SMALL_FONT_ID, m, footer + 24, tr(STR_WX_SOURCE));
  if (!view.unattended) {
    const auto back = footerButtonRect(w, h, metrics.contentSidePadding, false);
    const auto refresh = footerButtonRect(w, h, metrics.contentSidePadding, true);
    renderer.drawRect(back.x, back.y, back.width, back.height);
    renderer.drawRect(refresh.x, refresh.y, refresh.width, refresh.height);
    UITheme::drawCenteredText(renderer, Rect{back.x, back.y, back.width, back.height}, UI_10_FONT_ID, back.y + 12,
                              tr(STR_BACK));
    UITheme::drawCenteredText(renderer, Rect{refresh.x, refresh.y, refresh.width, refresh.height}, UI_10_FONT_ID,
                              refresh.y + 12, tr(STR_WX_REFRESH));
  }
}
}  // namespace weather
