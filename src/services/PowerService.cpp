#include "services/PowerService.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>
#include <esp_sleep.h>

#include "config.h"
#include "services/SplashService.h"

// Static member initialization
uint32_t PowerService::lastActivityTimestamp = 0;
bool PowerService::isInIdleState = false;

int PowerService::batteryPercent() {
  const int level = M5.Power.getBatteryLevel();
  if (level < 0) return -1;
  if (level > 100) return 100;
  return level;
}

void PowerService::recordActivity() {
  lastActivityTimestamp = millis();
  isInIdleState = false;
}

uint32_t PowerService::getIdleTime() {
  const uint32_t now = millis();
  // Handle rollover (after ~49 days)
  if (now >= lastActivityTimestamp) {
    return now - lastActivityTimestamp;
  }
  // Rollover case
  return (0xFFFFFFFF - lastActivityTimestamp) + now + 1;
}

bool PowerService::checkIdleTimeout() {
  return getIdleTime() >= IDLE_TIMEOUT_MS;
}

bool PowerService::isIdle() {
  return isInIdleState;
}

void PowerService::setIdleState(bool idle) {
  isInIdleState = idle;
}

void PowerService::enterDeepSleep(uint32_t wakeIntervalSeconds, bool showSplash) {
  Serial.println("Preparing for deep sleep...");
  Serial.printf("  Wake interval: %lu seconds\n", wakeIntervalSeconds);
  Serial.printf("  Show splash: %s\n", showSplash ? "yes" : "no");

  // 1. Show splash screen if enabled
  if (showSplash) {
    Serial.println("  Showing splash before sleep...");
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    SplashService::showQuickSplash();
  }

  // 2. Disconnect WiFi completely
  Serial.println("  Disconnecting WiFi...");
  WiFi.disconnect(true);  // true = also disable WiFi radio
  WiFi.mode(WIFI_OFF);
  delay(100);

  // 3. Unmount SD card
  Serial.println("  Unmounting SD card...");
  SD.end();

  // 4. Put display to sleep
  Serial.println("  Sleeping display...");
  M5.Display.sleep();

  // 5. Configure wake timer if interval specified
  if (wakeIntervalSeconds > 0) {
    Serial.printf("  Setting wake timer: %lu seconds\n", wakeIntervalSeconds);
    // Convert to microseconds for esp_sleep_enable_timer_wakeup
    esp_sleep_enable_timer_wakeup((uint64_t)wakeIntervalSeconds * 1000000ULL);
  }

  // 6. Enter deep sleep with touch wakeup enabled
  Serial.println("  Entering deep sleep...");
  Serial.flush();
  
  // Second parameter enables touch wakeup, power button wakes by default
  M5.Power.deepSleep(wakeIntervalSeconds > 0 ? wakeIntervalSeconds * 1000000ULL : 0, true);
}

PowerService::WakeCause PowerService::getWakeCause() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      return WakeCause::Timer;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return WakeCause::Touch;
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
      return WakeCause::Button;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return WakeCause::PowerOn;
  }
}
