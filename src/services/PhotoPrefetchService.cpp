#include "services/PhotoPrefetchService.h"

#include <HTTPClient.h>
#include <SD.h>

#include "services/PhotoCacheService.h"
#include "services/IoMutex.h"
#include "services/Memory.h"

PhotoPrefetchService::PhotoPrefetchService(PhotoApiClient& api, SDCardService& sd)
  : _api(api), _sd(sd) {}

void PhotoPrefetchService::start() {
  if (_running || _ready) return;

  _running = true;
  _lastError = String();

  // Ensure we're starting from a clean slate.
  clearFiles();

  // Run on the second core (ESP32-S3) to keep UI responsive.
  xTaskCreatePinnedToCore(taskEntry, "photo_prefetch", 8192, this, 1, nullptr, 1);
}

bool PhotoPrefetchService::tryConsume(PhotoInfo& outPhoto) {
  if (!_ready) return false;

  // Mark consumed first so a new prefetch can start after rendering.
  _ready = false;
  outPhoto = _photo;
  return true;
}

void PhotoPrefetchService::clearFiles() {
  IoGuard guard;
  if (_sd.ensureMounted()) {
    if (SD.exists(PhotoCacheService::kPrefetchImagePath)) SD.remove(PhotoCacheService::kPrefetchImagePath);
    if (SD.exists(PhotoCacheService::kPrefetchMetaPath)) SD.remove(PhotoCacheService::kPrefetchMetaPath);
  }
}

void PhotoPrefetchService::stop() {
  if (!_running && !_ready) return;
  
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

void PhotoPrefetchService::taskEntry(void* arg) {
  static_cast<PhotoPrefetchService*>(arg)->taskBody();
  vTaskDelete(nullptr);
}

void PhotoPrefetchService::taskBody() {
  PhotoInfo p;
  String err;

  // Fetch metadata + URL.
  if (!_api.fetchRandomPhoto(p, err)) {
    _lastError = err;
    _running = false;
    return;
  }

  // Download next image to the prefetch file.
  if (!_sd.ensureMounted()) {
    _lastError = "SD not mounted";
    _running = false;
    return;
  }

  if (!downloadToSdFile(p.url, PhotoCacheService::kPrefetchImagePath, 2048, err)) {
    _lastError = err;
    clearFiles();
    _running = false;
    return;
  }

  // Save metadata for the prefetched image.
  if (!PhotoCacheService::saveMeta(PhotoCacheService::kPrefetchMetaPath, p)) {
    _lastError = "Prefetch meta write failed";
    clearFiles();
    _running = false;
    return;
  }

  _photo = p;
  _ready = true;
  _running = false;
}

bool PhotoPrefetchService::downloadToSdFile(const String& url, const char* path, size_t bufferSize, String& outError) {
  IoGuard guard;

  HTTPClient http;
  http.begin(url);

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    outError = String("HTTP Error downloading image: ") + httpCode;
    http.end();
    return false;
  }

  // FILE_WRITE appends on Arduino SD; remove first to overwrite.
  if (SD.exists(path)) {
    SD.remove(path);
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
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
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      const int toRead = (size > bufferSize) ? bufferSize : static_cast<int>(size);
      const int c = stream->readBytes(buff, toRead);
      f.write(buff, c);
      if (len > 0) len -= c;
    }
    delay(1);
  }

  Memory::freeMem(buff);
  f.close();
  http.end();
  return true;
}
