#include "services/SplashService.h"

#include <M5Unified.h>
#include <JPEGDEC.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "ui/DisplayUI.h"
#include "services/SDCardService.h"
#include "services/IoMutex.h"

// Include the bitmap splash if enabled.
#if SPLASH_MODE == 1
#include "nameimage_2025_12_12_012855051.cpp"
#endif

namespace SplashService {

static void drawBitmap1Bit(const uint8_t* bitmap, int16_t w, int16_t h) {
  // Fast 1-bit horizontal bitmap rendering using pushImage.
  // Each byte = 8 horizontal pixels, MSB = leftmost pixel.
  const int32_t screenW = M5.Display.width();
  const int32_t screenH = M5.Display.height();
  const int32_t offsetX = (screenW - w) / 2;
  const int32_t offsetY = (screenH - h) / 2;

  // Disable auto-refresh during drawing to prevent scan effect.
  M5.Display.startWrite();

  // Process and push one row at a time to avoid huge buffer allocations.
  uint16_t* rowBuffer = (uint16_t*)malloc(w * sizeof(uint16_t));
  if (!rowBuffer) {
    Serial.println("Failed to allocate bitmap row buffer!");
    M5.Display.endWrite();
    return;
  }

  const int32_t bytesPerRow = (w + 7) / 8;
  for (int32_t y = 0; y < h; y++) {
    const size_t rowOffset = y * bytesPerRow;
    for (int32_t x = 0; x < w; x++) {
      const size_t byteIndex = rowOffset + (x / 8);
      const uint8_t bitMask = 0x80 >> (x % 8);
      const uint8_t pixel = pgm_read_byte(&bitmap[byteIndex]) & bitMask;
      rowBuffer[x] = pixel ? 0xFFFF : 0x0000;  // White or black in RGB565
    }
    M5.Display.pushImage(offsetX, offsetY + y, w, 1, rowBuffer);
  }

  free(rowBuffer);
  M5.Display.endWrite();
}

static int jpegDrawCallback(JPEGDRAW* pDraw) {
  // Direct rendering callback for JPEG decoder.
  const int32_t screenW = M5.Display.width();
  const int32_t screenH = M5.Display.height();
  const int32_t offsetX = (screenW - pDraw->iWidth) / 2;
  const int32_t offsetY = (screenH - pDraw->iHeight) / 2;
  M5.Display.pushImage(offsetX + pDraw->x, offsetY + pDraw->y, pDraw->iWidth, pDraw->iHeight, (uint16_t*)pDraw->pPixels);
  return 1;
}

static bool downloadSplashJpeg(const char* url, const char* path) {
  Serial.println(String("Downloading splash from: ") + url);
  
  IoGuard guard;
  HTTPClient http;
  http.begin(url);
  
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.println(String("HTTP error downloading splash: ") + httpCode);
    http.end();
    return false;
  }

  if (SD.exists(path)) {
    SD.remove(path);
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open splash file for writing");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buff[512];
  int totalBytes = 0;
  
  while (http.connected()) {
    size_t size = stream->available();
    if (size) {
      const int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      f.write(buff, c);
      totalBytes += c;
    }
    delay(1);
  }

  f.close();
  http.end();
  Serial.println(String("Splash downloaded: ") + totalBytes + " bytes");
  return true;
}

static void* jpegOpenFile(const char* filename, int32_t* size) {
  File* f = new File(SD.open(filename, FILE_READ));
  if (f && *f) {
    *size = f->size();
    return f;
  }
  delete f;
  return nullptr;
}

static void jpegCloseFile(void* handle) {
  File* f = (File*)handle;
  if (f) {
    f->close();
    delete f;
  }
}

static int32_t jpegReadFile(JPEGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  File* f = (File*)pFile->fHandle;
  return f ? f->read(pBuf, iLen) : 0;
}

static int32_t jpegSeekFile(JPEGFILE* pFile, int32_t iPosition) {
  File* f = (File*)pFile->fHandle;
  return (f && f->seek(iPosition)) ? 1 : 0;
}

static bool drawJpegFromSD(const char* path) {
  if (!SD.exists(path)) {
    Serial.println(String("JPEG splash not found: ") + path);
    return false;
  }

  JPEGDEC jpeg;
  if (jpeg.open(path, jpegOpenFile, jpegCloseFile, jpegReadFile, jpegSeekFile, jpegDrawCallback) != 1) {
    Serial.println("Failed to open JPEG splash");
    return false;
  }

  M5.Display.startWrite();
  jpeg.decode(0, 0, 0);
  M5.Display.endWrite();
  jpeg.close();
  return true;
}

void showBootSplash(const String& appName) {
  // Establish a known clean frame with a full refresh.
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.flush();

#if SPLASH_MODE == 2
  // Download JPEG splash if it doesn't exist on SD (first boot).
  if (!SD.exists(SPLASH_JPEG_PATH) && WiFi.status() == WL_CONNECTED) {
    Serial.println("First boot: downloading splash image...");
    downloadSplashJpeg(SPLASH_JPEG_URL, SPLASH_JPEG_PATH);
  }
  
  // Draw JPEG splash from SD card.
  if (!drawJpegFromSD(SPLASH_JPEG_PATH)) {
    // Fallback to text if JPEG fails.
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextDatum(datum_t::middle_center);
    M5.Display.drawString("Splash not found", M5.Display.width() / 2, M5.Display.height() / 2);
  }
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  delay(250);
#elif SPLASH_MODE == 1
  // Draw 1-bit bitmap splash.
  drawBitmap1Bit(splash_bitmap, SPLASH_BITMAP_WIDTH, SPLASH_BITMAP_HEIGHT);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  delay(250);
#else
  // Draw text splash.
  M5.Display.setTextColor(TFT_BLACK);
  switch (SPLASH_FONT) {
    case 0: M5.Display.setFont(&fonts::Font0); break;
    case 2: M5.Display.setFont(&fonts::Font2); break;
    case 4: M5.Display.setFont(&fonts::Font4); break;
    case 6: M5.Display.setFont(&fonts::Font6); break;
    case 8: M5.Display.setFont(&fonts::Font8); break;
    case 7: default: M5.Display.setFont(&fonts::Font7); break;
  }
  M5.Display.setTextDatum(datum_t::middle_center);
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.drawString(appName, cx, cy);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  delay(250);
#endif

  // Hold on the splash for the configured total duration.
  delay(SPLASH_TOTAL_MS);
  // Switch to fast mode for the rest of the boot UI.
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
}

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

} // namespace SplashService
