#include "services/WiFiService.h"

#include <WiFi.h>

#include "config.h"

namespace WifiService {

static uint32_t lastWifiUsedMs = 0;
static String storedSsid;
static String storedPassword;

bool connect(const char* ssid, const char* password, uint32_t timeoutMs) {
  // Store credentials for reconnection
  storedSsid = ssid;
  storedPassword = password;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    recordWifiUsage();
    return true;
  }
  return false;
}

void disconnect(bool turnOffRadio) {
  WiFi.disconnect(turnOffRadio);
  if (turnOffRadio) {
    WiFi.mode(WIFI_OFF);
  }
  Serial.println("WiFi disconnected");
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void recordWifiUsage() {
  lastWifiUsedMs = millis();
}

uint32_t getTimeSinceLastUsage() {
  const uint32_t now = millis();
  if (now >= lastWifiUsedMs) {
    return now - lastWifiUsedMs;
  }
  // Handle rollover
  return (0xFFFFFFFF - lastWifiUsedMs) + now + 1;
}

bool shouldDisconnectForIdle() {
#if DISCONNECT_WIFI_ON_IDLE
  return isConnected() && getTimeSinceLastUsage() >= WIFI_IDLE_DISCONNECT_MS;
#else
  return false;
#endif
}

bool reconnect(uint32_t timeoutMs) {
  if (storedSsid.isEmpty()) {
    Serial.println("No stored WiFi credentials for reconnection");
    return false;
  }
  Serial.println("Reconnecting to WiFi...");
  return connect(storedSsid.c_str(), storedPassword.c_str(), timeoutMs);
}

} // namespace WifiService
