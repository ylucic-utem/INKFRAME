---
description: Implement hybrid power mode with auto-sleep after 10 minutes of inactivity
---

# Power Management Enhancement Implementation

This workflow implements a hybrid power mode that allows active user interaction while automatically entering deep sleep after ten minutes of inactivity.

## Phase 1: Configuration Updates

### Step 1.1: Add Power Management Constants to `config.h`

Add the following configuration parameters to `include/config.h`:

```cpp
// ============ Power Management Configuration ============
// Idle timeout before entering deep sleep (10 minutes = 600,000 ms)
#define IDLE_TIMEOUT_MS 600000

// Deep sleep wake interval for automatic updates (12 hours = 43200 seconds)
#define DEEP_SLEEP_WAKE_INTERVAL_SECONDS 43200

// Show splash screen before entering sleep
#define SHOW_SPLASH_ON_SLEEP true

// Disconnect WiFi when approaching idle (saves power before full sleep)
#define DISCONNECT_WIFI_ON_IDLE true

// WiFi idle disconnect threshold (2 minutes before going idle)
#define WIFI_IDLE_DISCONNECT_MS 120000

// Idle warning time (seconds before sleep to show warning)
#define IDLE_WARNING_SECONDS 30

// Critical battery percentage threshold
#define BATTERY_CRITICAL_PERCENT 15

// Extended wake interval for low battery (48 hours)
#define LOW_BATTERY_WAKE_INTERVAL_SECONDS 172800
```

---

## Phase 2: PowerService Enhancement

### Step 2.1: Update `include/services/PowerService.h`

Replace the entire file with:

```cpp
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
```

### Step 2.2: Update `src/services/PowerService.cpp`

Replace the entire file with:

```cpp
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
    SplashService::showBootSplash(String(APP_NAME));
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
```

---

## Phase 3: WiFiService Enhancement

### Step 3.1: Update `include/services/WiFiService.h`

Replace the entire file with:

```cpp
#pragma once

#include <Arduino.h>

namespace WifiService {
  // Connect to WiFi with timeout
  bool connect(const char* ssid, const char* password, uint32_t timeoutMs);
  
  // Disconnect WiFi and optionally turn off radio
  void disconnect(bool turnOffRadio = true);
  
  // Check if connected
  bool isConnected();
  
  // Track WiFi usage for idle disconnection
  void recordWifiUsage();
  uint32_t getTimeSinceLastUsage();
  bool shouldDisconnectForIdle();
  
  // Reconnect using stored credentials
  bool reconnect(uint32_t timeoutMs);
}
```

### Step 3.2: Update `src/services/WiFiService.cpp`

Replace the entire file with:

```cpp
#include "services/WiFiService.h"

#include <WiFi.h>

#include "config.h"

namespace WifiService {

static uint32_t lastWifiUsedMs = 0;
static String storedSsid;
static String storedPassword;

bool connect(const char* ssid, const char* password, uint32_t timeoutMs) {
  // Store credentials for reconnection
  storedSsid = ssid;
  storedPassword = password;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    recordWifiUsage();
    return true;
  }
  return false;
}

void disconnect(bool turnOffRadio) {
  WiFi.disconnect(turnOffRadio);
  if (turnOffRadio) {
    WiFi.mode(WIFI_OFF);
  }
  Serial.println("WiFi disconnected");
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void recordWifiUsage() {
  lastWifiUsedMs = millis();
}

uint32_t getTimeSinceLastUsage() {
  const uint32_t now = millis();
  if (now >= lastWifiUsedMs) {
    return now - lastWifiUsedMs;
  }
  // Handle rollover
  return (0xFFFFFFFF - lastWifiUsedMs) + now + 1;
}

bool shouldDisconnectForIdle() {
#if DISCONNECT_WIFI_ON_IDLE
  return isConnected() && getTimeSinceLastUsage() >= WIFI_IDLE_DISCONNECT_MS;
#else
  return false;
#endif
}

bool reconnect(uint32_t timeoutMs) {
  if (storedSsid.isEmpty()) {
    Serial.println("No stored WiFi credentials for reconnection");
    return false;
  }
  return connect(storedSsid.c_str(), storedPassword.c_str(), timeoutMs);
}

} // namespace WifiService
```

---

## Phase 4: PhotoPrefetchService Enhancement

### Step 4.1: Update `include/services/PhotoPrefetchService.h`

Add the `stop()` method to the class:

```cpp
// After the existing public methods, add:
  
  // Safely stop the prefetch task
  void stop();
```

### Step 4.2: Update `src/services/PhotoPrefetchService.cpp`

Add the stop method implementation:

```cpp
// Add after clearFiles():

void PhotoPrefetchService::stop() {
  if (!_running) return;
  
  Serial.println("Stopping prefetch service...");
  
  // Mark as not running - the task will exit on next check
  _running = false;
  _ready = false;
  
  // Clean up any partial downloads
  clearFiles();
  
  // Give the task time to finish
  delay(100);
  
  Serial.println("Prefetch service stopped");
}
```

---

## Phase 5: SplashService Enhancement

### Step 5.1: Update `include/services/SplashService.h`

Add a quick splash method for sleep transitions:

```cpp
namespace SplashService {

// Performs an explicit full refresh to a white background, then plays the splash animation.
// This is intended to run once on boot.
void showBootSplash(const String& appName);

// Quick splash display for sleep transitions (no long delays)
void showQuickSplash();

} // namespace SplashService
```

### Step 5.2: Update `src/services/SplashService.cpp`

Add the quick splash implementation at the end of the namespace, before the closing brace:

```cpp
void showQuickSplash() {
  // Use epd_quality for a clean final image before sleep
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.fillScreen(TFT_WHITE);
  
#if SPLASH_MODE == 1
  // Draw 1-bit bitmap splash (fast - from compiled memory)
  drawBitmap1Bit(splash_bitmap, SPLASH_BITMAP_WIDTH, SPLASH_BITMAP_HEIGHT);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
#elif SPLASH_MODE == 2
  // Draw JPEG splash from SD card
  if (!drawJpegFromSD(SPLASH_JPEG_PATH)) {
    // Fallback to app name
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&fonts::Font7);
    M5.Display.setTextDatum(datum_t::middle_center);
    M5.Display.drawString(APP_NAME, M5.Display.width() / 2, M5.Display.height() / 2);
    M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  }
#else
  // Draw text splash
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextDatum(datum_t::middle_center);
  M5.Display.drawString(APP_NAME, M5.Display.width() / 2, M5.Display.height() / 2);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
#endif

  // Short delay to ensure display completes
  delay(100);
}
```

---

## Phase 6: DisplayUI Enhancement

### Step 6.1: Update `include/ui/DisplayUI.h`

Add idle warning function:

```cpp
// After the existing function declarations, add:

  // Idle warning display
  void showIdleWarning(uint32_t secondsRemaining);
  void clearIdleWarning();
```

### Step 6.2: Update `src/ui/DisplayUI.cpp`

Add the idle warning implementation:

```cpp
// Add before the closing namespace brace:

static bool idleWarningVisible = false;

void showIdleWarning(uint32_t secondsRemaining) {
  if (secondsRemaining == 0) {
    clearIdleWarning();
    return;
  }
  
  // Draw warning in taskbar area
  const Rect bar = taskbarRect();
  const int32_t warningX = sleepButtonRect().x + sleepButtonRect().w + 10;
  const int32_t warningY = bar.y + 8;
  
  // Draw sleep icon (ZZZ) and countdown
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  
  String warning = String("Sleep in ") + secondsRemaining + "s";
  
  // Clear previous text
  M5.Display.fillRect(warningX, warningY, 150, 20, TFT_WHITE);
  M5.Display.setCursor(warningX, warningY);
  M5.Display.print(warning);
  
  // Only refresh if not already showing (avoid constant refreshes)
  if (!idleWarningVisible) {
    Rect warnRect = makeRect(warningX, warningY, 150, 20);
    refreshRect(warnRect);
    idleWarningVisible = true;
  }
}

void clearIdleWarning() {
  if (!idleWarningVisible) return;
  
  const Rect bar = taskbarRect();
  const int32_t warningX = sleepButtonRect().x + sleepButtonRect().w + 10;
  const int32_t warningY = bar.y + 8;
  
  M5.Display.fillRect(warningX, warningY, 150, 20, TFT_WHITE);
  idleWarningVisible = false;
}
```

---

## Phase 7: Main.cpp Integration

### Step 7.1: Add new includes and state variables

At the top of main.cpp, after existing includes:

```cpp
#include <esp_sleep.h>
```

After the existing static variables (around line 39), add:

```cpp
// Power management state
static bool timerWakeMode = false;  // True if woken by timer (vs user interaction)
static uint32_t lastIdleWarningSecond = 0;
```

### Step 7.2: Add helper function for sleep transition

Add this function before `setup()`:

```cpp
static void enterIdleSleep() {
  Serial.println("Entering idle sleep transition...");
  
  // Stop prefetch service
  prefetchService.stop();
  
  // Disconnect WiFi
  WifiService::disconnect(true);
  
  // Determine wake interval based on battery level
  uint32_t wakeInterval = DEEP_SLEEP_WAKE_INTERVAL_SECONDS;
  const int batt = PowerService::batteryPercent();
  if (batt >= 0 && batt < BATTERY_CRITICAL_PERCENT) {
    wakeInterval = LOW_BATTERY_WAKE_INTERVAL_SECONDS;
    Serial.printf("Low battery (%d%%), using extended wake interval\n", batt);
  }
  
  // Enter deep sleep with splash
  PowerService::enterDeepSleep(wakeInterval, SHOW_SPLASH_ON_SLEEP);
}
```

### Step 7.3: Update setup() function

At the **end** of the existing `setup()` function, add:

```cpp
  // Initialize activity tracking
  PowerService::recordActivity();
  
  // Check if this is a timer wake (for background updates)
  PowerService::WakeCause wakeCause = PowerService::getWakeCause();
  Serial.print("Wake cause: ");
  switch (wakeCause) {
    case PowerService::WakeCause::Timer:
      Serial.println("TIMER");
      timerWakeMode = true;
      break;
    case PowerService::WakeCause::Touch:
      Serial.println("TOUCH");
      break;
    case PowerService::WakeCause::Button:
      Serial.println("BUTTON");
      break;
    default:
      Serial.println("POWER_ON");
      break;
  }
  
  // If woken by timer, do a background update and go back to sleep
  if (timerWakeMode) {
    Serial.println("Timer wake: fetching new photo and returning to sleep");
    if (WiFi.status() == WL_CONNECTED || WifiService::reconnect(WIFI_TIMEOUT_MS)) {
      fetchAndShowNextPhoto();
    }
    // Return to sleep
    enterIdleSleep();
    return;  // Never reached, but for clarity
  }
```

### Step 7.4: Update loop() function

Replace the `loop()` function with:

```cpp
void loop() {
  M5.update();

  // Check idle timeout BEFORE processing input
  const uint32_t idleTime = PowerService::getIdleTime();
  const uint32_t timeToSleep = (idleTime < IDLE_TIMEOUT_MS) ? (IDLE_TIMEOUT_MS - idleTime) : 0;
  const uint32_t secondsToSleep = timeToSleep / 1000;
  
  // Show idle warning in last 30 seconds
  if (secondsToSleep <= IDLE_WARNING_SECONDS && secondsToSleep > 0) {
    // Only update display once per second to save power
    if (secondsToSleep != lastIdleWarningSecond) {
      lastIdleWarningSecond = secondsToSleep;
      DisplayUI::showIdleWarning(secondsToSleep);
    }
  } else if (lastIdleWarningSecond != 0) {
    // Clear warning if user interacted
    DisplayUI::clearIdleWarning();
    lastIdleWarningSecond = 0;
  }
  
  // Check if idle timeout exceeded
  if (PowerService::checkIdleTimeout() && !PowerService::isIdle()) {
    Serial.println("Idle timeout reached - entering sleep");
    PowerService::setIdleState(true);
    enterIdleSleep();
    return;  // Never reached
  }
  
  // Proactive WiFi disconnect when approaching idle
  if (WifiService::shouldDisconnectForIdle()) {
    Serial.println("WiFi idle disconnect");
    WifiService::disconnect(false);  // Keep radio on for quick reconnect
  }

  switch (inputHandler.poll()) {
    case InputHandler::Action::Next:
      Serial.println("NEXT requested");
      PowerService::recordActivity();
      DisplayUI::clearIdleWarning();
      lastIdleWarningSecond = 0;
      
      // Reconnect WiFi if disconnected
      if (!WifiService::isConnected()) {
        WifiService::reconnect(WIFI_TIMEOUT_MS);
      }
      WifiService::recordWifiUsage();
      
      fetchAndShowNextPhoto();
      break;
      
    case InputHandler::Action::Sleep:
      Serial.println("SLEEP requested - immediate sleep");
      PowerService::recordActivity();  // Record to ensure clean state
      enterIdleSleep();
      break;
      
    case InputHandler::Action::ToggleQuality: {
      PowerService::recordActivity();
      DisplayUI::clearIdleWarning();
      lastIdleWarningSecond = 0;
      
      qualityModeEnabled = !qualityModeEnabled;
      Serial.println(String("QUALITY toggled: ") + (qualityModeEnabled ? "ON" : "OFF"));

      // Re-render the current (last) photo in the newly selected mode.
      if (!sdCard.ensureMounted() || !SD.exists(PhotoCacheService::kLastImagePath)) {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        DisplayUI::showBanner("No cached photo");
        DisplayUI::drawTaskbar(currentPhoto.title, qualityModeEnabled);
        DisplayUI::refreshRect(DisplayUI::taskbarRect());
        break;
      }

      String err;
      if (!imagePipeline.renderLocalPhoto(currentPhoto, PhotoCacheService::kLastImagePath, qualityModeEnabled, err)) {
        Serial.println("Quality toggle re-render failed: " + err);
      }
      break;
    }
    case InputHandler::Action::None:
    default:
      break;
  }

  delay(100);
}
```

---

## Phase 8: Update fetchAndShowNextPhoto

### Step 8.1: Track WiFi usage in API calls

In the `fetchAndShowNextPhoto()` function, add WiFi usage tracking after successful API calls:

```cpp
// After successful API fetch (around line 177-178), add:
WifiService::recordWifiUsage();

// After successful prefetch consumption, add:
WifiService::recordWifiUsage();
```

---

## Phase 9: Testing Checklist

// turbo-all

### 9.1: Build the project
```
pio run -e m5stack-papers3
```

### 9.2: Upload and monitor
```
pio run -e m5stack-papers3 -t upload && pio device monitor -b 115200
```

### Test Scenarios:
1. **Fresh boot**: Should show splash, load photo, initialize activity timer
2. **User interaction**: Pressing Next/Quality should reset idle timer
3. **Idle warning**: After 9.5 minutes, countdown should appear in taskbar
4. **Auto sleep**: After 10 minutes of no interaction, device should sleep
5. **Touch wake**: Touch screen should wake device and show last photo
6. **Timer wake**: After wake interval, device should update photo and sleep
7. **Low battery**: When battery < 15%, extended wake interval should be used
8. **Manual sleep**: SLEEP button should immediately enter sleep mode

---

## Notes

- The implementation uses RTC_DATA_ATTR-capable static variables in PowerService
- WiFi credentials are stored for reconnection after idle disconnect
- Error handling during sleep transition ensures device sleeps even if splash fails
- Prefetch service is stopped before sleep to prevent partial downloads
- Timer wake mode bypasses normal UI and immediately updates + sleeps
