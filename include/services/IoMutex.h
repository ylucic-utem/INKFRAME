#pragma once

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class IoMutex {
public:
  // One global mutex for SD/FS operations.
  static SemaphoreHandle_t handle();
};

class IoGuard {
public:
  IoGuard();
  ~IoGuard();

  IoGuard(const IoGuard&) = delete;
  IoGuard& operator=(const IoGuard&) = delete;

private:
  SemaphoreHandle_t _m;
  bool _locked;
};
