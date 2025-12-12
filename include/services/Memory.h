#pragma once

#include <Arduino.h>

namespace Memory {

// Best-effort allocator that prefers PSRAM when available.
// Returns nullptr on failure.
void* mallocPreferPsram(size_t size);

// Convenience typed allocation.
template <typename T>
inline T* mallocArrayPreferPsram(size_t count) {
  return static_cast<T*>(mallocPreferPsram(sizeof(T) * count));
}

inline void freeMem(void* p) {
  free(p);
}

} // namespace Memory
