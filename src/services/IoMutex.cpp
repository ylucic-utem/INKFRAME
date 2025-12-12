#include "services/IoMutex.h"

SemaphoreHandle_t IoMutex::handle() {
  static SemaphoreHandle_t s_mutex = nullptr;
  if (!s_mutex) {
    s_mutex = xSemaphoreCreateMutex();
  }
  return s_mutex;
}

IoGuard::IoGuard() : _m(IoMutex::handle()), _locked(false) {
  if (_m) {
    xSemaphoreTake(_m, portMAX_DELAY);
    _locked = true;
  }
}

IoGuard::~IoGuard() {
  if (_m && _locked) {
    xSemaphoreGive(_m);
  }
}
