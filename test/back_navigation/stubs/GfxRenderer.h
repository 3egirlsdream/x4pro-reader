#pragma once
struct EpdFontFamily {
  enum Style { REGULAR };
};
class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  Orientation orientation = Portrait;
  int panelWidth = 800, panelHeight = 480;
  Orientation getOrientation() const { return orientation; }
  int getScreenWidth() const { return orientation == Portrait || orientation == PortraitInverted ? 480 : 800; }
  int getScreenHeight() const { return orientation == Portrait || orientation == PortraitInverted ? 800 : 480; }
  void tapToLogical(float nx, float ny, int& x, int& y) const;
};
