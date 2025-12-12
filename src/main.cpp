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

// Cache navigation state
static int16_t currentCachePosition = -1;  // -1 = viewing live/most recent, >=0 = viewing cached history
static bool offlineModeActive = false;

static void fetchAndShowNextPhoto();
static bool showLastPhotoOnBoot();
static void enterIdleSleep();
static void showCachedPhotoAtPosition(uint8_t position);
static void updateTaskbarWithCacheInfo();
static bool checkOfflineMode();

// Helper to enter idle/deep sleep with proper cleanup
static void enterIdleSleep() {
  Serial.println("Entering idle sleep transition...");
  
  // Stop prefetch service
  prefetchService.stop();
  
  // Save cache state before sleeping
  PhotoCacheService::saveCacheIndex();
  
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

// Check if we should enter offline mode
static bool checkOfflineMode() {
  bool wifiConnected = WifiService::isConnected();
  bool hasCache = PhotoCacheService::hasOfflineContent();
  
  if (!wifiConnected && hasCache) {
    if (!offlineModeActive) {
      Serial.println("Entering offline mode - WiFi unavailable but cache has photos");
      offlineModeActive = true;
      DisplayUI::showOfflineIndicator(true);
    }
    return true;
  }
  
  if (wifiConnected && offlineModeActive) {
    Serial.println("Exiting offline mode - WiFi reconnected");
    offlineModeActive = false;
    DisplayUI::showOfflineIndicator(false);
  }
  
  return false;
}

// Show a cached photo at the given position
static void showCachedPhotoAtPosition(uint8_t position) {
  PhotoInfo photo;
  String imagePath;
  
  if (!PhotoCacheService::getFromCache(position, photo, imagePath)) {
    Serial.printf("Failed to get cached photo at position %d\n", position);
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::showBanner("Cache error", "Photo not found");
    return;
  }
  
  // Record access for LRU tracking
  PhotoCacheService::recordAccess(position);
  
  Serial.printf("Showing cached photo at position %d: %s\n", position, photo.title.c_str());
  
  // Render the cached photo
  String err;
  if (!imagePipeline.renderLocalPhoto(photo, imagePath.c_str(), qualityModeEnabled, err)) {
    Serial.println("Failed to render cached photo: " + err);
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::showBanner("Render error", err);
    return;
  }
  
  currentPhoto = photo;
  currentCachePosition = position;
  
  updateTaskbarWithCacheInfo();
}

// Update taskbar with current cache position info
static void updateTaskbarWithCacheInfo() {
  uint8_t count = PhotoCacheService::getCachedPhotoCount();
  uint8_t displayPosition = (currentCachePosition >= 0) ? currentCachePosition : 
                             PhotoCacheService::getCurrentCacheIndex();
  
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::drawTaskbar(currentPhoto.title, qualityModeEnabled, 
                         displayPosition, count, offlineModeActive);
  DisplayUI::refreshRect(DisplayUI::taskbarRect());
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

  // Initialize cache system early
  if (!PhotoCacheService::initCache()) {
    Serial.println("WARNING: Cache initialization failed");
  } else {
    Serial.printf("Cache initialized: %d photos available\n", 
                  PhotoCacheService::getCachedPhotoCount());
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
    // Check if we can use offline mode
    checkOfflineMode();
  }

  // Prefetch next image in the background so "Next" is instant.
  if (wifiOk) {
    prefetchService.start();
  }

  // If there was no cached image to show, fetch one now.
  if (!showedLast) {
    if (wifiOk) {
      // Still on splash: now fetch and show next photo (this will draw taskbar when done).
      fetchAndShowNextPhoto();
    } else if (PhotoCacheService::hasOfflineContent()) {
      // Offline mode: show most recent cached photo
      uint8_t mostRecent = PhotoCacheService::getCurrentCacheIndex();
      showCachedPhotoAtPosition(mostRecent);
    } else {
      // No WiFi and no cache - show error
      M5.Display.setEpdMode(epd_mode_t::epd_fastest);
      DisplayUI::showBanner("No connection", "No cached photos");
    }
  }

  // If we did show a cached image above, now draw the taskbar persistently.
  if (showedLast) {
    // Set position to most recent
    currentCachePosition = PhotoCacheService::getCurrentCacheIndex();
    updateTaskbarWithCacheInfo();
  }
  
  // Initialize activity tracking
  PowerService::recordActivity();
  Serial.println("Activity tracking initialized");
  
  // Run cache validation in background during boot idle time
  PhotoCacheService::runMaintenance(500);  // 500ms budget
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
    checkOfflineMode();  // Update offline mode status
  }

  switch (inputHandler.poll()) {
    case InputHandler::Action::Next: {
      Serial.println("NEXT requested");
      PowerService::recordActivity();
      DisplayUI::clearIdleWarning();
      lastIdleWarningSecond = 0;
      
      // Check if we're in offline mode or viewing cached history
      if (offlineModeActive) {
        // Offline: navigate forward through cache only
        uint8_t count = PhotoCacheService::getCachedPhotoCount();
        if (count > 0) {
          int16_t nextPos = currentCachePosition + 1;
          if (nextPos < count) {
            showCachedPhotoAtPosition(nextPos);
          } else {
            // At the end of cache, wrap to beginning
            showCachedPhotoAtPosition(0);
          }
        }
      } else if (currentCachePosition >= 0 && 
                 currentCachePosition < PhotoCacheService::getCurrentCacheIndex()) {
        // Viewing history: move forward through cache
        showCachedPhotoAtPosition(currentCachePosition + 1);
      } else {
        // At most recent position or no cache: fetch new photo from API
        // Reconnect WiFi if disconnected
        if (!WifiService::isConnected()) {
          WifiService::reconnect(WIFI_TIMEOUT_MS);
        }
        WifiService::recordWifiUsage();
        
        fetchAndShowNextPhoto();
      }
      break;
    }
    
    case InputHandler::Action::Previous: {
      Serial.println("PREVIOUS requested");
      PowerService::recordActivity();
      DisplayUI::clearIdleWarning();
      lastIdleWarningSecond = 0;
      
      uint8_t count = PhotoCacheService::getCachedPhotoCount();
      if (count == 0) {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        DisplayUI::showBanner("No history", "No cached photos");
        break;
      }
      
      // Navigate backward through cache
      int16_t prevPos;
      if (currentCachePosition < 0) {
        // Currently at live/most recent - go to previous
        prevPos = PhotoCacheService::getCurrentCacheIndex();
        if (prevPos > 0) {
          prevPos--;
        } else {
          // Wrap to end of cache
          prevPos = count - 1;
        }
      } else if (currentCachePosition > 0) {
        prevPos = currentCachePosition - 1;
      } else {
        // At beginning of cache, wrap to end
        prevPos = count - 1;
      }
      
      showCachedPhotoAtPosition(prevPos);
      break;
    }
      
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

      // Determine which image file to re-render
      String imagePath;
      if (currentCachePosition >= 0) {
        // Viewing cached photo
        PhotoInfo photo;
        if (PhotoCacheService::getFromCache(currentCachePosition, photo, imagePath)) {
          String err;
          if (!imagePipeline.renderLocalPhoto(photo, imagePath.c_str(), qualityModeEnabled, err)) {
            Serial.println("Quality toggle re-render failed: " + err);
          }
        }
      } else {
        // Viewing most recent
        if (!sdCard.ensureMounted() || !SD.exists(PhotoCacheService::kLastImagePath)) {
          M5.Display.setEpdMode(epd_mode_t::epd_fastest);
          DisplayUI::showBanner("No cached photo");
          updateTaskbarWithCacheInfo();
          break;
        }

        String err;
        if (!imagePipeline.renderLocalPhoto(currentPhoto, PhotoCacheService::kLastImagePath, qualityModeEnabled, err)) {
          Serial.println("Quality toggle re-render failed: " + err);
        }
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
    
    // Check if we can fallback to offline mode
    if (PhotoCacheService::hasOfflineContent()) {
      DisplayUI::showBanner("WiFi unavailable", "Browsing cache");
      offlineModeActive = true;
      DisplayUI::showOfflineIndicator(true);
      uint8_t mostRecent = PhotoCacheService::getCurrentCacheIndex();
      showCachedPhotoAtPosition(mostRecent);
    } else {
      DisplayUI::showBanner("WiFi not connected");
    }
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
      
      // Save to multi-image cache
      PhotoCacheService::saveToCache(currentPhoto, PhotoCacheService::kLastImagePath);
      currentCachePosition = PhotoCacheService::getCurrentCacheIndex();
      
      updateTaskbarWithCacheInfo();
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
    
    // Fallback to cache if available
    if (PhotoCacheService::hasOfflineContent()) {
      delay(1500);  // Show error briefly
      offlineModeActive = true;
      uint8_t mostRecent = PhotoCacheService::getCurrentCacheIndex();
      showCachedPhotoAtPosition(mostRecent);
    }
    return;
  }

  DisplayUI::clearProgress();
  WifiService::recordWifiUsage();  // API call used WiFi
  offlineModeActive = false;
  DisplayUI::showOfflineIndicator(false);

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

  // Save to multi-image cache
  PhotoCacheService::saveToCache(currentPhoto, PhotoCacheService::kLastImagePath);
  currentCachePosition = PhotoCacheService::getCurrentCacheIndex();

  imageChangeCount = nextIndex;
  updateTaskbarWithCacheInfo();
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