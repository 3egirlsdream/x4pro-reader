#pragma once
struct CrossPointSettings {
  void saveToFile() {}
  enum { PREV_NEXT, NEXT_PREV, PREV_PREV, NEXT_NEXT, SIDE_BUTTONS_DISABLED };
  enum SHORT_PWRBTN { PWR_CONFIRM, PWR_SLEEP };
  int frontButtonBack = 0, frontButtonConfirm = 1, frontButtonLeft = 2, frontButtonRight = 3;
  int sideButtonLayout = PREV_NEXT, shortPwrBtn = PWR_SLEEP;
  int homeButtonTapAction = 0, homeButtonDoubleTapAction = 0, homeButtonLongPressAction = 0;
  bool frontButtonFollowOrientation = false, doubleClickPwrLight = true;
  int getPowerButtonDuration() const { return 1000; }
};
inline CrossPointSettings SETTINGS;
