#include "services/PowerService.h"

#include <M5Unified.h>

int PowerService::batteryPercent() {
  const int level = M5.Power.getBatteryLevel();
  if (level < 0) return -1;
  if (level > 100) return 100;
  return level;
}

void PowerService::enterDeepSleep() {
  // Sleep indefinitely. Disable touch wakeup to avoid immediate wake after tapping the button.
  M5.Display.sleep();
  M5.Power.deepSleep(0, false);
}
