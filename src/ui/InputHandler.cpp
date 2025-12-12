#include "ui/InputHandler.h"

#include <M5Unified.h>

#include "ui/DisplayUI.h"
#include "config.h"

InputHandler::InputHandler(uint32_t debounceMs, bool touchEnabled, float touchThresholdXRatio)
  : _debounceMs(debounceMs), _touchEnabled(touchEnabled), _touchThresholdXRatio(touchThresholdXRatio) {}

static bool pointInRect(int32_t x, int32_t y, const DisplayUI::Rect& r) {
  return x >= r.x && y >= r.y && x < (r.x + r.w) && y < (r.y + r.h);
}

InputHandler::Action InputHandler::poll() {
  _wasLongPress = false;
  
  // Hardware button A -> Next photo
  if (M5.BtnA.wasPressed()) {
    _lastTriggerMs = millis();
    return Action::Next;
  }

  if (_touchEnabled) {
    auto touch = M5.Touch.getDetail();
    
    // Track touch start for long-press detection
    if (touch.state == m5::touch_state_t::touch_begin) {
      _touchStartMs = millis();
      _touchActive = true;
      _lastTouchX = touch.x;
      _lastTouchY = touch.y;
    }
    
    // Check for touch events
    if (touch.state == m5::touch_state_t::touch || 
        touch.state == m5::touch_state_t::touch_end) {
      
      // Debounce check
      if ((millis() - _lastTriggerMs) <= _debounceMs) {
        if (touch.state == m5::touch_state_t::touch_end) {
          _touchActive = false;
        }
        return Action::None;
      }
      
      // Store touch position for feedback
      _lastTouchX = touch.x;
      _lastTouchY = touch.y;
      
      // Get button regions
      const auto nextR = DisplayUI::nextButtonRect();
      const auto prevR = DisplayUI::previousButtonRect();
      const auto sleepR = DisplayUI::sleepButtonRect();
      const auto qualityR = DisplayUI::qualityButtonRect();
      const auto infoR = DisplayUI::infoButtonRect();
      
      // Calculate touch duration for long-press
      uint32_t touchDuration = millis() - _touchStartMs;
      
      // Only process on touch_end for reliable press detection
      if (touch.state == m5::touch_state_t::touch_end && _touchActive) {
        _touchActive = false;
        
        // Check for long press on quality button -> ToggleInfo
        if (pointInRect(touch.x, touch.y, qualityR)) {
          if (touchDuration >= LONG_PRESS_MS) {
            _wasLongPress = true;
            _lastTriggerMs = millis();
            return Action::ToggleInfo;
          }
          _lastTriggerMs = millis();
          return Action::ToggleQuality;
        }
        
        // Check info button (if visible)
        if (pointInRect(touch.x, touch.y, infoR)) {
          _lastTriggerMs = millis();
          return Action::ToggleInfo;
        }

        if (pointInRect(touch.x, touch.y, nextR)) {
          _lastTriggerMs = millis();
          return Action::Next;
        }

        if (pointInRect(touch.x, touch.y, prevR)) {
          _lastTriggerMs = millis();
          return Action::Previous;
        }

        if (pointInRect(touch.x, touch.y, sleepR)) {
          _lastTriggerMs = millis();
          return Action::Sleep;
        }
        
        // Touch in image area (above taskbar) - toggle info
        const auto imgR = DisplayUI::imageRect();
        if (pointInRect(touch.x, touch.y, imgR)) {
          if (touchDuration >= LONG_PRESS_MS) {
            _wasLongPress = true;
            _lastTriggerMs = millis();
            return Action::ToggleInfo;
          }
        }
      }
      
      // Handle immediate touch (for buttons that need instant feedback)
      if (touch.state == m5::touch_state_t::touch) {
        // Just show feedback, don't trigger action yet
      }
    }
    
    // Reset touch state on release
    if (touch.state == m5::touch_state_t::none || touch.state == m5::touch_state_t::touch_end) {
      _touchActive = false;
    }
  }

  return Action::None;
}

