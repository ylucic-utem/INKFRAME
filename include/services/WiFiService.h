#pragma once

#include <Arduino.h>

namespace WifiService {
  // Connect to WiFi with timeout
  bool connect(const char* ssid, const char* password, uint32_t timeoutMs);
  
  // Disconnect WiFi and optionally turn off radio
  void disconnect(bool turnOffRadio = true);
  
  // Check if connected
  bool isConnected();
  
  // Track WiFi usage for idle disconnection
  void recordWifiUsage();
  uint32_t getTimeSinceLastUsage();
  bool shouldDisconnectForIdle();
  
  // Reconnect using stored credentials
  bool reconnect(uint32_t timeoutMs);
}
