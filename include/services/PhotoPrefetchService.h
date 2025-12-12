#pragma once

#include <Arduino.h>

#include "services/PhotoApiClient.h"
#include "services/PhotoTypes.h"
#include "services/SDCardService.h"

class PhotoPrefetchService {
public:
  PhotoPrefetchService(PhotoApiClient& api, SDCardService& sd);

  // Starts a background fetch+download if not already running/ready.
  void start();

  bool isRunning() const { return _running; }
  bool hasReady() const { return _ready; }

  // If a prefetched photo is ready, returns it and clears the ready flag.
  bool tryConsume(PhotoInfo& outPhoto);

  // Best-effort cleanup of the prefetched files (used after failures).
  void clearFiles();
  
  // Safely stop the prefetch task
  void stop();

private:
  static void taskEntry(void* arg);
  void taskBody();

  bool downloadToSdFile(const String& url, const char* path, size_t bufferSize, String& outError);

  PhotoApiClient& _api;
  SDCardService& _sd;

  volatile bool _running = false;
  volatile bool _ready = false;

  PhotoInfo _photo;
  String _lastError;
};
