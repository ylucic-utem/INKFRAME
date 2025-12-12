#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>
#include <esp_sleep.h>

#include "config.h"

#include "services/ImagePipeline.h"
#include "services/PhotoApiClient.h"
#include "services/PhotoCacheService.h"
#include "services/PhotoPrefetchService.h"
#include "services/PhotoTypes.h"
#include "services/PowerService.h"
#include "services/SDCardService.h"
#include "services/SplashService.h"
#include "services/WiFiService.h"

#include "ui/DisplayUI.h"
#include "ui/InputHandler.h"

// PaperS3 SD Card SPI Pins (CRITICAL - must match PaperS3 hardware)
// These are the CORRECT pins for M5Stack PaperS3 - different from older M5Paper!
#define SD_SPI_CS_PIN   47
#define SD_SPI_SCK_PIN  39
#define SD_SPI_MOSI_PIN 38
#define SD_SPI_MISO_PIN 40

// WiFi credentials - CONFIGURE IN config.h
static const char* SSID = WIFI_SSID;
static const char* PASSWORD = WIFI_PASSWORD;

static SDCardService sdCard(SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN);
static PhotoApiClient apiClient(PHOTO_API_URL, API_TIMEOUT_MS);
static InputHandler inputHandler(BUTTON_DEBOUNCE_MS, BUTTON_ENABLE_TOUCH, TOUCH_THRESHOLD_X_RATIO);
static ImagePipeline imagePipeline(sdCard);
static PhotoPrefetchService prefetchService(apiClient, sdCard);

static PhotoInfo currentPhoto;
static uint32_t imageChangeCount = 0;
static bool qualityModeEnabled = false;

// Power management state
static bool timerWakeMode = false;  // True if woken by timer (vs user interaction)
static uint32_t lastIdleWarningSecond = 0;

static void fetchAndShowNextPhoto();
static bool showLastPhotoOnBoot();
static void enterIdleSleep();

// Helper to enter idle/deep sleep with proper cleanup
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


void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // Set display rotation to landscape mode (90° clockwise)
  // Rotation values: 0=portrait, 1=landscape(90°CW), 2=portrait-inverted, 3=landscape-inverted
  M5.Display.setRotation(1);
  
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\nM5Stack PaperS3 Photo Viewer Starting...");

  // Check wake cause early
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

  if (!sdCard.begin()) {
    Serial.println("ERROR: SD Card mount failed!");
    delay(600);
  } else {
    Serial.println("SUCCESS: SD Card mounted");
  }

  // Connect WiFi early (needed for splash download on first boot).
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(SSID);
  const bool wifiOk = WifiService::connect(SSID, PASSWORD, WIFI_TIMEOUT_MS);

  // If timer wake mode, do background update and return to sleep
  if (timerWakeMode) {
    Serial.println("Timer wake: fetching new photo and returning to sleep");
    if (wifiOk) {
      // Try to show last photo first for quick display update
      showLastPhotoOnBoot();
      // Fetch a new photo
      fetchAndShowNextPhoto();
    }
    // Return to sleep
    enterIdleSleep();
    return;  // Never reached, but for clarity
  }
  
  // Normal boot: Show splash (will download from URL if first boot and WiFi is connected).
  SplashService::showBootSplash(String(APP_NAME));

  // Try to show the last downloaded image quickly.
  const bool showedLast = showLastPhotoOnBoot();
  if (wifiOk) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
    // Keep splash visible; we'll proceed offline if possible.
  }

  // Prefetch next image in the background so "Next" is instant.
  if (wifiOk) {
    prefetchService.start();
  }

  // If there was no cached image to show, fetch one now.
  if (!showedLast) {
    // Still on splash: now fetch and show next photo (this will draw taskbar when done).
    fetchAndShowNextPhoto();
  }

  // If we did show a cached image above, now draw the taskbar persistently.
  if (showedLast) {
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::drawTaskbar(currentPhoto.title, qualityModeEnabled);
    DisplayUI::refreshRect(DisplayUI::taskbarRect());
  }
  
  // Initialize activity tracking
  PowerService::recordActivity();
  Serial.println("Activity tracking initialized");
}

void loop() {
  M5.update();

  // Check idle timeout BEFORE processing input
  const uint32_t idleTime = PowerService::getIdleTime();
  const uint32_t timeToSleep = (idleTime < IDLE_TIMEOUT_MS) ? (IDLE_TIMEOUT_MS - idleTime) : 0;
  const uint32_t secondsToSleep = timeToSleep / 1000;
  
  // Show idle warning in last 30 seconds
  if (secondsToSleep <= IDLE_WARNING_SECONDS && secondsToSleep > 0) {
    // Only update display when seconds change
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
        // Keep taskbar state in sync.
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

static void fetchAndShowNextPhoto() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::showBanner("WiFi not connected");
    return;
  }

  // Cancel any in-progress request before starting a new one
  if (apiClient.isRequestInProgress()) {
    Serial.println("Cancelling previous request...");
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::showBanner("Cancelling...");
    apiClient.cancelCurrentRequest();
    delay(100);  // Brief pause for visual feedback
  }

  // Record WiFi usage for idle tracking
  WifiService::recordWifiUsage();

  const uint32_t nextIndex = imageChangeCount + 1;
  const bool useQuality = qualityModeEnabled;

  PhotoInfo prefetched;
  if (prefetchService.tryConsume(prefetched)) {
    Serial.println("Using prefetched photo");
    currentPhoto = prefetched;
    WifiService::recordWifiUsage();  // Prefetch used WiFi

    String renderErr;
    if (imagePipeline.renderPrefetchedPhoto(currentPhoto, useQuality, renderErr)) {
      imageChangeCount = nextIndex;
      prefetchService.start();
      return;
    }

    Serial.println("Prefetched render failed: " + renderErr);
    prefetchService.clearFiles();
    // Fall through to synchronous fetch.
  }

  Serial.println("Fetching photo from API...");
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::showProgress("Loading...", 0);

  // Progress callback for visual feedback
  auto progressCallback = [](int progress) {
    DisplayUI::showProgress("Loading...", progress);
  };

  String err;
  if (!apiClient.fetchRandomPhoto(currentPhoto, err, progressCallback)) {
    Serial.println("API fetch failed: " + err);
    DisplayUI::clearProgress();

    // Log request metadata for diagnostics
    const RequestMetadata& meta = apiClient.getLastRequestMetadata();
    Serial.printf("Request #%lu: %s after %lu ms, %d retries, HTTP %d\n",
                  meta.requestNumber,
                  meta.cancelled ? "CANCELLED" : "FAILED",
                  meta.endTime - meta.startTime,
                  meta.retryCount,
                  meta.httpCode);

    // Handle specific error categories with appropriate messages
    if (meta.cancelled) {
      // Request was cancelled, no error banner needed
      return;
    } else if (err.indexOf("Unauthorized") >= 0 || err.indexOf("403") >= 0) {
      DisplayUI::showBanner("API error", "Check credentials");
    } else if (err.indexOf("Rate limited") >= 0 || err.indexOf("429") >= 0) {
      DisplayUI::showBanner("API error", "Rate limited - wait");
    } else if (err.indexOf("Not found") >= 0 || err.indexOf("404") >= 0) {
      DisplayUI::showBanner("API error", "Photo not found");
    } else if (err.startsWith("JSON Parse Error")) {
      DisplayUI::showBanner("API error", "JSON parse");
    } else if (err.startsWith("HTTP Error") || err.indexOf("HTTP") >= 0) {
      DisplayUI::showBanner("API error", err);
    } else if (err.indexOf("No photos") >= 0) {
      DisplayUI::showBanner("API error", "No photos");
    } else if (err.indexOf("timeout") >= 0 || err.indexOf("Timeout") >= 0) {
      DisplayUI::showBanner("Network error", "Connection timeout");
    } else if (err.indexOf("Connection") >= 0) {
      DisplayUI::showBanner("Network error", err);
    } else {
      DisplayUI::showBanner("API error", err);
    }
    return;
  }

  DisplayUI::clearProgress();
  WifiService::recordWifiUsage();  // API call used WiFi

  // Log successful request metadata
  const RequestMetadata& meta = apiClient.getLastRequestMetadata();
  Serial.printf("Request #%lu: SUCCESS in %lu ms, %d retries\n",
                meta.requestNumber,
                meta.endTime - meta.startTime,
                meta.retryCount);

  Serial.println("Photo URL: " + currentPhoto.url);
  Serial.println("Title: " + currentPhoto.title);
  Serial.println("Description: " + currentPhoto.description);

  String renderErr;
  if (!imagePipeline.renderPhoto(currentPhoto, useQuality, renderErr)) {
    Serial.println("Render failed: " + renderErr);
    // ImagePipeline already displayed an on-screen error; nothing else needed.
    return;
  }

  imageChangeCount = nextIndex;
  prefetchService.start();
}

static bool showLastPhotoOnBoot() {
  if (!sdCard.ensureMounted()) {
    return false;
  }

  if (!SD.exists(PhotoCacheService::kLastImagePath)) {
    return false;
  }

  PhotoInfo last;
  (void)PhotoCacheService::loadMeta(PhotoCacheService::kLastMetaPath, last);

  String err;
  // Boot render should be fast.
  if (!imagePipeline.renderLocalPhoto(last, PhotoCacheService::kLastImagePath, qualityModeEnabled, err)) {
    Serial.println("Failed to render last photo: " + err);
    return false;
  }

  currentPhoto = last;
  return true;
}