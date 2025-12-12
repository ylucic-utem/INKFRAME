#pragma once

#include <Arduino.h>

namespace WifiService {
  bool connect(const char* ssid, const char* password, uint32_t timeoutMs);
}
