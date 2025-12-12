#pragma once

#include <Arduino.h>

class SDCardService {
public:
  SDCardService(int csPin, int sckPin, int misoPin, int mosiPin, uint32_t mountFreqHz = 25000000, uint32_t remountFreqHz = 10000000);

  bool begin();
  bool ensureMounted();
  bool isMounted() const { return _mounted; }

private:
  bool ensurePhotosDir();

  int _csPin;
  int _sckPin;
  int _misoPin;
  int _mosiPin;
  uint32_t _mountFreqHz;
  uint32_t _remountFreqHz;
  bool _mounted = false;
};
