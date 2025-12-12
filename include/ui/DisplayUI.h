#pragma once

#include <Arduino.h>

namespace DisplayUI {
  struct Rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
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
  Rect qualityButtonRect();
  Rect sleepButtonRect();

  // UI
  void drawTaskbar(const String& title, bool qualityEnabled);

  // Banner (white, centered) + partial refresh helpers
  void showBanner(const String& line1);
  void showBanner(const String& line1, const String& line2);
  void clearBanner();

  // Idle warning display
  void showIdleWarning(uint32_t secondsRemaining);
  void clearIdleWarning();

  // Progress bar display (for download/network operations)
  void showProgress(const String& label, int percentage);
  void clearProgress();

  void refreshRect(const Rect& r);
}
