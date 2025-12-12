#pragma once

#include <Arduino.h>

class InputHandler {
public:
  enum class Action {
    None = 0,
    Next,
    Sleep,
    ToggleQuality,
  };

  InputHandler(uint32_t debounceMs, bool touchEnabled, float touchThresholdXRatio);

  // Call after M5.update(); returns user action.
  Action poll();

private:
  uint32_t _lastTriggerMs = 0;
  uint32_t _debounceMs;
  bool _touchEnabled;
  float _touchThresholdXRatio;
};
