#include "services/WiFiService.h"

#include <WiFi.h>

#include "config.h"
#include "ui/DisplayUI.h"

namespace WifiService {

static uint32_t lastWifiUsedMs = 0;
static String storedSsid;
static String storedPassword;

// Update DisplayUI with current WiFi state
static void updateDisplayWifiState() {
  wl_status_t status = WiFi.status();
  DisplayUI::WifiState displayState;
  
  switch (status) {
    case WL_CONNECTED:
      displayState = DisplayUI::WifiState::Connected;
      break;
    case WL_IDLE_STATUS:
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED:
      displayState = DisplayUI::WifiState::Connecting;
      break;
    case WL_CONNECT_FAILED:
    case WL_CONNECTION_LOST:
      displayState = DisplayUI::WifiState::Error;
      break;
    case WL_DISCONNECTED:
    case WL_NO_SHIELD:
    default:
      displayState = DisplayUI::WifiState::Disconnected;
      break;
  }
  
  DisplayUI::setWifiState(displayState);
}

bool connect(const char* ssid, const char* password, uint32_t timeoutMs) {
  // Store credentials for reconnection
  storedSsid = ssid;
  storedPassword = password;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Show connecting state
  DisplayUI::setWifiState(DisplayUI::WifiState::Connecting);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    recordWifiUsage();
    DisplayUI::setWifiState(DisplayUI::WifiState::Connected);
    return true;
  }
  
  // Connection failed
  DisplayUI::setWifiState(DisplayUI::WifiState::Error);
  return false;
}

void disconnect(bool turnOffRadio) {
  WiFi.disconnect(turnOffRadio);
  if (turnOffRadio) {
    WiFi.mode(WIFI_OFF);
  }
  Serial.println("WiFi disconnected");
  DisplayUI::setWifiState(DisplayUI::WifiState::Disconnected);
}

bool isConnected() {
  bool connected = WiFi.status() == WL_CONNECTED;
  
  // Update display state opportunistically
  updateDisplayWifiState();
  
  return connected;
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

// Get current WiFi RSSI strength (for potential signal bars)
int getSignalStrength() {
  if (WiFi.status() != WL_CONNECTED) {
    return -100;  // No signal
  }
  return WiFi.RSSI();
}

} // namespace WifiService

