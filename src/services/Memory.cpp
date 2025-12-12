#include "services/Memory.h"

#if defined(ESP32)
  #include <esp_heap_caps.h>
  #include <esp32-hal-psram.h>
#endif

namespace Memory {

void* mallocPreferPsram(size_t size) {
#if defined(ESP32)
  // psramFound() is available on Arduino-ESP32 and works on ESP32-S3.
  if (psramFound()) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) return p;
  }
#endif
  return malloc(size);
}

} // namespace Memory
