#pragma once

#include <Arduino.h>
#include "services/PhotoTypes.h"

namespace DisplayUI {
  struct Rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
  };

  // WiFi connection states for icon display
  enum class WifiState {
    Connected,
    Connecting,
    Disconnected,
    Error
  };

  // Splash screen (full-screen, white background).
  // Shows a centered app name in a large font, optionally animating characters.
  void showSplash(const String& appName);
  void showSplashAnimated(const String& appName, uint32_t perCharDelayMs = 140, uint32_t endHoldMs = 600);
  // Simple variant: just draw large centered name and push full frame.
  void showSplashSimple(const String& appName);

  void showTwoLineStatus(const String& line1, const String& line2);
  void showSingleLineStatus(const String& line1);

  // Regions
  Rect taskbarRect();
  Rect imageRect();
  Rect bannerRect();

  int32_t taskbarHeight();
  Rect nextButtonRect();
  Rect previousButtonRect();   // Previous button for cache navigation
  Rect qualityButtonRect();
  Rect sleepButtonRect();
  Rect infoButtonRect();       // NEW: Info toggle button

  // ============ Battery Icon ============
  // Draw a battery icon with fill level (0-100%)
  // Shows warning indicators for low/critical levels
  void drawBatteryIcon(int32_t x, int32_t y, int percentage, bool charging = false);
  
  // Smart battery refresh - returns true if icon was updated
  bool updateBatteryIconIfNeeded();

  // ============ WiFi Status Icon ============
  // Draw WiFi icon reflecting connection state
  void drawWifiIcon(int32_t x, int32_t y, WifiState state);
  
  // Update WiFi state and redraw if changed
  void setWifiState(WifiState state);
  WifiState getWifiState();

  // ============ Progress Bar ============
  // Draw a download progress bar (0-100%)
  // For indeterminate progress, pass -1
  void drawProgressBar(int32_t x, int32_t y, int32_t width, int32_t height, int percentage);
  
  // Centered progress bar with label (uses epd_fastest for partial updates)
  void showProgress(const String& label, int percentage);
  void clearProgress();

  // ============ Photo Info Overlay ============
  // Show metadata overlay on current photo
  void showPhotoInfo(const PhotoInfo& photo, uint32_t downloadTimestamp = 0, 
                     int32_t width = 0, int32_t height = 0);
  void hidePhotoInfo();
  bool isPhotoInfoVisible();

  // ============ Button Feedback ============
  // Flash visual feedback when button pressed
  void showButtonFeedback(const Rect& buttonRect);
  
  // Flash corner indicator for quick acknowledgment
  void showTouchFeedback();

  // ============ Operation Status Text ============
  // Update status text in taskbar center (e.g., "Loading...", "Cached")
  void setStatusText(const String& status);
  void clearStatusText();

  // ============ Rendering Animation ============
  // Show animated rendering indicator ("Rendering.", "Rendering..", etc.)
  void showRenderingAnimation(int step);  // 0-2 for dot animation
  void hideRenderingAnimation();

  // ============ Cache Position Visualization ============
  // Show dot indicator for cache position (position is 0-indexed)
  void drawCachePositionIndicator(uint8_t position, uint8_t total);

  // ============ Error States ============
  // Draw error banner with type-specific icon
  enum class ErrorType {
    Network,
    SDCard,
    API,
    Render,
    Generic
  };
  void showErrorBanner(const String& title, const String& message, ErrorType type);

  // UI - Taskbar variants
  void drawTaskbar(const String& title, bool qualityEnabled);
  
  // Extended taskbar with cache position indicator
  void drawTaskbar(const String& title, bool qualityEnabled, 
                   uint8_t cachePosition, uint8_t cacheCount,
                   bool offlineMode = false);

  // Full-featured taskbar with all status indicators
  void drawTaskbarFull(const String& title, bool qualityEnabled,
                       uint8_t cachePosition, uint8_t cacheCount,
                       bool offlineMode, WifiState wifiState,
                       int batteryPercent, bool batteryCharging);

  // Banner (white, centered) + partial refresh helpers
  void showBanner(const String& line1);
  void showBanner(const String& line1, const String& line2);
  void clearBanner();

  // Idle warning display
  void showIdleWarning(uint32_t secondsRemaining);
  void clearIdleWarning();

  // Offline mode indicator
  void showOfflineIndicator(bool show);

  void refreshRect(const Rect& r);
}

