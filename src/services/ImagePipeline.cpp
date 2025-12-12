#include "services/ImagePipeline.h"

#include <algorithm>
#include <cmath>

#include <M5Unified.h>
#include <HTTPClient.h>
#include <SD.h>
#include <time.h>

#include "services/PhotoCacheService.h"
#include "services/IoMutex.h"
#include "services/Memory.h"
#include "ui/DisplayUI.h"

namespace {
uint16_t readU16BE(File& f) {
  uint8_t b[2] = {0, 0};
  if (f.read(b, 2) != 2) {
    return 0;
  }
  return (static_cast<uint16_t>(b[0]) << 8) | static_cast<uint16_t>(b[1]);
}

bool readJpegDimensionsFromFile(const char* path, int& outW, int& outH, String& outError) {
  outW = 0;
  outH = 0;

  File f = SD.open(path, FILE_READ);
  if (!f) {
    outError = "JPEG open failed";
    return false;
  }

  // Check SOI marker 0xFFD8
  uint8_t soi[2] = {0, 0};
  if (f.read(soi, 2) != 2 || soi[0] != 0xFF || soi[1] != 0xD8) {
    f.close();
    outError = "Not a JPEG (missing SOI)";
    return false;
  }

  while (f.available()) {
    // Find 0xFF marker prefix
    int c = f.read();
    if (c < 0) break;
    if (static_cast<uint8_t>(c) != 0xFF) continue;

    // Skip fill bytes 0xFF
    uint8_t marker = 0;
    do {
      c = f.read();
      if (c < 0) {
        marker = 0;
        break;
      }
      marker = static_cast<uint8_t>(c);
    } while (marker == 0xFF);

    if (marker == 0) break;

    // Standalone markers without length
    if (marker == 0xD8 || marker == 0xD9) continue;               // SOI / EOI
    if (marker >= 0xD0 && marker <= 0xD7) continue;               // RSTn
    if (marker == 0x01) continue;                                 // TEM
    if (marker == 0xDA) break;                                     // SOS (start of scan) => no more headers

    const uint16_t segLen = readU16BE(f);
    if (segLen < 2) {
      break;
    }

    const bool isSOF =
      (marker == 0xC0) || (marker == 0xC1) || (marker == 0xC2) || (marker == 0xC3) ||
      (marker == 0xC5) || (marker == 0xC6) || (marker == 0xC7) ||
      (marker == 0xC9) || (marker == 0xCA) || (marker == 0xCB) ||
      (marker == 0xCD) || (marker == 0xCE) || (marker == 0xCF);

    if (isSOF) {
      // Segment format: [precision:1][height:2][width:2]...
      (void)f.read();
      const uint16_t h = readU16BE(f);
      const uint16_t w = readU16BE(f);
      f.close();
      outW = static_cast<int>(w);
      outH = static_cast<int>(h);
      return (outW > 0 && outH > 0);
    }

    // Skip remainder of segment
    const uint32_t toSkip = static_cast<uint32_t>(segLen) - 2;
    if (!f.seek(f.position() + toSkip)) {
      break;
    }
  }

  f.close();
  outError = "JPEG dimensions not found";
  return false;
}
}

ImagePipeline::ImagePipeline(SDCardService& sd) : _sd(sd) {}

bool ImagePipeline::renderPhoto(const PhotoInfo& photo, bool useQuality, String& outError) {
  const auto prevMode = M5.Display.getEpdMode();

  // Progress banner (keep existing content; refresh only a small region).
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  Serial.println("Fetching image: " + photo.url);
  DisplayUI::showBanner("Downloading...");

  if (!downloadToSdFile(photo.url, PhotoCacheService::kStagingImagePath, 2048, outError)) {
    DisplayUI::showBanner("Download error", outError);
    M5.Display.setEpdMode(prevMode);
    return false;
  }

  // Render stage banner
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::showBanner("Rendering...");

  // For the final image, switch between speed/quality modes.
  M5.Display.setEpdMode(useQuality ? epd_mode_t::epd_quality : epd_mode_t::epd_fast);
  if (!renderJpegFromSdFile(PhotoCacheService::kStagingImagePath, outError)) {
    DisplayUI::showBanner("Decode error", outError);
    M5.Display.setEpdMode(prevMode);
    return false;
  }

  // Taskbar (update title + battery) and refresh only the bar region.
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::drawTaskbar(photo.title, useQuality);
  DisplayUI::refreshRect(DisplayUI::taskbarRect());

  String promoteErr;
  if (!PhotoCacheService::promoteToLast(PhotoCacheService::kStagingImagePath, nullptr, promoteErr)) {
    Serial.println("Failed to promote staging -> last: " + promoteErr);
    // Non-fatal: the photo was already displayed.
  } else {
    if (!PhotoCacheService::saveMeta(PhotoCacheService::kLastMetaPath, photo)) {
      Serial.println("Failed to write last.meta");
    }
  }

  saveMetadata(photo);
  M5.Display.setEpdMode(prevMode);
  return true;
}

bool ImagePipeline::renderPrefetchedPhoto(const PhotoInfo& photo, bool useQuality, String& outError) {
  const auto prevMode = M5.Display.getEpdMode();

  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::showBanner("Rendering...");

  M5.Display.setEpdMode(useQuality ? epd_mode_t::epd_quality : epd_mode_t::epd_fast);
  if (!renderJpegFromSdFile(PhotoCacheService::kPrefetchImagePath, outError)) {
    DisplayUI::showBanner("Decode error", outError);
    M5.Display.setEpdMode(prevMode);
    return false;
  }

  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::drawTaskbar(photo.title, useQuality);
  DisplayUI::refreshRect(DisplayUI::taskbarRect());

  String promoteErr;
  if (!PhotoCacheService::promoteToLast(PhotoCacheService::kPrefetchImagePath, PhotoCacheService::kPrefetchMetaPath, promoteErr)) {
    Serial.println("Failed to promote prefetch -> last: " + promoteErr);
    // Non-fatal: already displayed.
  }

  saveMetadata(photo);
  M5.Display.setEpdMode(prevMode);
  return true;
}

bool ImagePipeline::renderLocalPhoto(const PhotoInfo& photo, const char* imagePath, bool useQuality, String& outError) {
  const auto prevMode = M5.Display.getEpdMode();

  // Avoid showing a banner if we can; just render.
  M5.Display.setEpdMode(useQuality ? epd_mode_t::epd_quality : epd_mode_t::epd_fast);
  if (!renderJpegFromSdFile(imagePath, outError)) {
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    DisplayUI::showBanner("Decode error", outError);
    M5.Display.setEpdMode(prevMode);
    return false;
  }

  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  DisplayUI::drawTaskbar(photo.title, useQuality);
  DisplayUI::refreshRect(DisplayUI::taskbarRect());

  M5.Display.setEpdMode(prevMode);
  return true;
}

bool ImagePipeline::downloadToSdFile(const String& url, const char* path, size_t bufferSize, String& outError) {
  IoGuard guard;

  HTTPClient http;
  http.begin(url);

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    outError = String("HTTP Error downloading image: ") + httpCode;
    http.end();
    return false;
  }

  Serial.println("Image downloaded successfully");
  Serial.printf("Free heap before SD operation: %d bytes\n", ESP.getFreeHeap());

  if (!_sd.ensureMounted()) {
    Serial.println("ERROR: Could not access SD card!");
    http.end();

    DisplayUI::showBanner("SD Card error", "Check SD card");

    outError = "SD Card Error";
    return false;
  }

  // FILE_WRITE appends on Arduino SD; remove first to overwrite.
  if (SD.exists(path)) {
    SD.remove(path);
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open SD card file for writing - checking SD mount");
    Serial.println("SD Card detected: " + String(SD.cardType() == CARD_NONE ? "NO" : "YES"));

    DisplayUI::showBanner("SD write error", String("File: ") + path);

    outError = "SD open for write failed";
    http.end();
    return false;
  }

  int len = http.getSize();
  uint8_t* buff = Memory::mallocArrayPreferPsram<uint8_t>(bufferSize);
  if (!buff) {
    f.close();
    outError = "Out of memory (download buffer)";
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int totalBytes = 0;

  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      const int toRead = (size > bufferSize) ? bufferSize : static_cast<int>(size);
      const int c = stream->readBytes(buff, toRead);
      f.write(buff, c);
      totalBytes += c;
      if (len > 0) {
        len -= c;
      }
    }
    delay(1);
  }

  Memory::freeMem(buff);
  f.close();
  http.end();

  Serial.println(String("Image saved to SD card: ") + totalBytes + " bytes");
  return true;
}

bool ImagePipeline::renderJpegFromSdFile(const char* path, String& outError) {
  IoGuard guard;

  const auto vp = DisplayUI::imageRect();
  M5.Display.fillRect(vp.x, vp.y, vp.w, vp.h, TFT_WHITE);

  // Read image dimensions from the JPEG header (fast; avoids loading the full file into RAM).
  int imgW = 0;
  int imgH = 0;
  if (!readJpegDimensionsFromFile(path, imgW, imgH, outError)) {
    Serial.println("Failed to read JPEG dimensions: " + outError);
    return false;
  }

  // Aspect-fill (center-crop): keep aspect ratio, fill the image viewport.
  const float scale = std::max(static_cast<float>(vp.w) / static_cast<float>(imgW),
                               static_cast<float>(vp.h) / static_cast<float>(imgH));
  const int32_t scaledW = static_cast<int32_t>(lroundf(static_cast<float>(imgW) * scale));
  const int32_t scaledH = static_cast<int32_t>(lroundf(static_cast<float>(imgH) * scale));
  const int32_t dstX = vp.x + (vp.w - scaledW) / 2;
  const int32_t dstY = vp.y + (vp.h - scaledH) / 2;

  // Use the File* overload instead of drawJpgFile(SD, ...) because the SDFS
  // filesystem type can fail the DataWrapperT<fs::SDFS> instantiation on some cores.
  File jpgFile = SD.open(path, FILE_READ);
  if (!jpgFile) {
    Serial.println("Failed to open JPEG file from SD (for rendering)");
    outError = "JPEG open failed";
    return false;
  }

  // Clip to the viewport to avoid drawing into the taskbar.
  M5.Display.setClipRect(vp.x, vp.y, vp.w, vp.h);
  const bool ok = M5.Display.drawJpg(&jpgFile, dstX, dstY, 0, 0, 0, 0, scale, scale, datum_t::top_left);
  M5.Display.clearClipRect();

  jpgFile.close();

  if (ok) {
    Serial.println("JPEG displayed successfully!");
    // Refresh only the image region.
    DisplayUI::refreshRect(vp);
    return true;
  }

  Serial.println("Failed to draw JPEG");
  outError = "JPEG decode failed";
  return false;
}

void ImagePipeline::saveMetadata(const PhotoInfo& photo) {
  if (!_sd.ensureMounted()) {
    Serial.println("savePhotoToSD: SD card not available");
    return;
  }

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char filename[80];
  strftime(filename, sizeof(filename), "/PHOTOS/photo_%Y%m%d_%H%M%S.txt", timeinfo);

  File f = SD.open(filename, FILE_WRITE);
  if (f) {
    f.println("Title: " + photo.title);
    f.println("Description: " + photo.description);
    f.println("URL: " + photo.url);
    f.println("Downloaded: " + String(filename));
    f.close();

    Serial.println("Metadata saved to: " + String(filename));
  } else {
    Serial.println("Failed to open file for writing: " + String(filename));
  }
}
