#pragma once

#include <Arduino.h>

class InputHandler {
public:
  enum class Action {
    None = 0,
    Next,
    Previous,      // Navigate to previous cached photo
    Sleep,
    ToggleQuality,
    ToggleInfo,    // NEW: Toggle photo info overlay
  };

  InputHandler(uint32_t debounceMs, bool touchEnabled, float touchThresholdXRatio);

  // Call after M5.update(); returns user action.
  Action poll();
  
  // Check if last action was from a long press
  bool wasLongPress() const { return _wasLongPress; }
  
  // Get the last touch position (for feedback)
  int32_t getLastTouchX() const { return _lastTouchX; }
  int32_t getLastTouchY() const { return _lastTouchY; }

private:
  uint32_t _lastTriggerMs = 0;
  uint32_t _debounceMs;
  bool _touchEnabled;
  float _touchThresholdXRatio;
  
  // Long press tracking
  uint32_t _touchStartMs = 0;
  bool _touchActive = false;
  bool _wasLongPress = false;
  int32_t _lastTouchX = 0;
  int32_t _lastTouchY = 0;
};

