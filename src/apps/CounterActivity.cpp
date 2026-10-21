#include "CounterActivity.h"

#include <cstdio>

void CounterActivity::onEnter() {
  JsonDocument doc;
  if (loadData(doc) && (doc["schema"] | 0) == 1) value = doc["value"] | int32_t(0);
  savedValue = value;
  updateRows();
  NativeAppActivity::onEnter();
}
void CounterActivity::onExit() {
  if (value != savedValue) {
    JsonDocument doc;
    doc["schema"] = 1;
    doc["value"] = value;
    if (!saveData(doc)) LOG_ERR("APP", "Counter state not saved");
  }
  NativeAppActivity::onExit();
}
void CounterActivity::updateRows() {
  snprintf(valueText, sizeof(valueText), "%ld", static_cast<long>(value));
  rowCount = 3;
  rows[0].label = tr(STR_APP_INCREMENT);
  rows[0].subtitle = valueText;
  rows[1].label = tr(STR_APP_DECREMENT);
  rows[2].label = tr(STR_APP_RESET);
  for (uint16_t i = 0; i < rowCount; ++i) rows[i].actionValue = i;
}
void CounterActivity::activateIndex(int index) {
  RenderLock lock(*this);
  if (index == 0 && value < INT32_MAX) ++value;
  if (index == 1 && value > INT32_MIN) --value;
  if (index == 2) value = 0;
  updateRows();
  requestUpdate();
}
