#pragma once

#include <Arduino.h>

class PowerService {
public:
  // Returns battery percentage 0-100 (or -1 if not available).
  static int batteryPercent();

  // Enters deep sleep indefinitely.
  static void enterDeepSleep();
};
