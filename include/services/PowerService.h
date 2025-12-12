#pragma once

#include <Arduino.h>

class PowerService {
public:
  // Returns battery percentage 0-100 (or -1 if not available).
  static int batteryPercent();

  // Activity tracking
  static void recordActivity();
  static uint32_t getIdleTime();
  static bool checkIdleTimeout();
  
  // Sleep state
  static bool isIdle();
  static void setIdleState(bool idle);

  // Deep sleep with configurable wake
  // wakeIntervalSeconds: 0 = indefinite sleep, >0 = wake after N seconds
  // showSplash: true = display splash before sleeping
  static void enterDeepSleep(uint32_t wakeIntervalSeconds = 0, bool showSplash = true);

  // Wake cause detection
  enum class WakeCause {
    PowerOn,       // Fresh boot or power button
    Timer,         // Scheduled wake from timer
    Touch,         // User touched screen
    Button,        // User pressed button
    Unknown
  };
  static WakeCause getWakeCause();

private:
  static uint32_t lastActivityTimestamp;
  static bool isInIdleState;
};
