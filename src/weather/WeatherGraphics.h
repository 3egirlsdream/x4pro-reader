#pragma once
class GfxRenderer;
namespace weather::graphics {
enum class WxCategory { Clear, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Storm };
void drawBigText(const GfxRenderer& renderer, int x, int y, const char* text, int dot);
int bigTextWidth(const char* text, int dot);
void drawWeatherIcon(const GfxRenderer& renderer, WxCategory category, int x, int y, int size);
}  // namespace weather::graphics
