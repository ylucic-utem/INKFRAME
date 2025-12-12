#include "ui/InputHandler.h"

#include <M5Unified.h>

#include "ui/DisplayUI.h"

InputHandler::InputHandler(uint32_t debounceMs, bool touchEnabled, float touchThresholdXRatio)
  : _debounceMs(debounceMs), _touchEnabled(touchEnabled), _touchThresholdXRatio(touchThresholdXRatio) {}

static bool pointInRect(int32_t x, int32_t y, const DisplayUI::Rect& r) {
  return x >= r.x && y >= r.y && x < (r.x + r.w) && y < (r.y + r.h);
}

InputHandler::Action InputHandler::poll() {
  // Hardware button A -> Next photo
  if (M5.BtnA.wasPressed()) {
    _lastTriggerMs = millis();
    return Action::Next;
  }

  if (_touchEnabled) {
    auto touch = M5.Touch.getDetail();
    if (touch.state == m5::touch_state_t::touch) {
      if ((millis() - _lastTriggerMs) <= _debounceMs) {
        return Action::None;
      }

      const auto nextR = DisplayUI::nextButtonRect();
      const auto prevR = DisplayUI::previousButtonRect();
      const auto sleepR = DisplayUI::sleepButtonRect();
      const auto qualityR = DisplayUI::qualityButtonRect();

      if (pointInRect(touch.x, touch.y, nextR)) {
        _lastTriggerMs = millis();
        return Action::Next;
      }

      if (pointInRect(touch.x, touch.y, prevR)) {
        _lastTriggerMs = millis();
        return Action::Previous;
      }

      if (pointInRect(touch.x, touch.y, qualityR)) {
        _lastTriggerMs = millis();
        return Action::ToggleQuality;
      }

      if (pointInRect(touch.x, touch.y, sleepR)) {
        _lastTriggerMs = millis();
        return Action::Sleep;
      }
    }
  }

  return Action::None;
}
