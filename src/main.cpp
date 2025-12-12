#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>

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

static void fetchAndShowNextPhoto();
static bool showLastPhotoOnBoot();


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
  
  // Show splash (will download from URL if first boot and WiFi is connected).
  SplashService::showBootSplash(String(APP_NAME));

  // Try to show the last downloaded image quickly.
  const bool showedLast = showLastPhotoOnBoot();
  if (wifiOk) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
    // Keep splash visible; we’ll proceed offline if possible.
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
}

void loop() {
  M5.update();

  switch (inputHandler.poll()) {
    case InputHandler::Action::Next:
      Serial.println("NEXT requested");
      fetchAndShowNextPhoto();
      break;
    case InputHandler::Action::Sleep:
      Serial.println("SLEEP requested");
      PowerService::enterDeepSleep();
      break;
    case InputHandler::Action::ToggleQuality: {
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

  const uint32_t nextIndex = imageChangeCount + 1;
  const bool useQuality = qualityModeEnabled;

  PhotoInfo prefetched;
  if (prefetchService.tryConsume(prefetched)) {
    Serial.println("Using prefetched photo");
    currentPhoto = prefetched;

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
  DisplayUI::showBanner("Loading...");

  String err;
  if (!apiClient.fetchRandomPhoto(currentPhoto, err)) {
    Serial.println("API fetch failed: " + err);

    if (err.startsWith("JSON Parse Error")) {
      DisplayUI::showBanner("API error", "JSON parse");
    } else if (err.startsWith("HTTP Error")) {
      DisplayUI::showBanner("API error", err);
    } else if (err.indexOf("No photos") >= 0) {
      DisplayUI::showBanner("API error", "No photos");
    } else {
      DisplayUI::showBanner("API error", err);
    }
    return;
  }

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