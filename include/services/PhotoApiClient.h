#pragma once

#include <Arduino.h>

#include "services/PhotoTypes.h"

class PhotoApiClient {
public:
  PhotoApiClient(const char* apiUrl, uint32_t timeoutMs = 30000);

  bool fetchRandomPhoto(PhotoInfo& outPhoto, String& outError);

private:
  const char* _apiUrl;
  uint32_t _timeoutMs;
};
