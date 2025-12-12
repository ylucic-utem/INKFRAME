#include "services/SDCardService.h"

#include <SPI.h>
#include <SD.h>

SDCardService::SDCardService(int csPin, int sckPin, int misoPin, int mosiPin, uint32_t mountFreqHz, uint32_t remountFreqHz)
  : _csPin(csPin), _sckPin(sckPin), _misoPin(misoPin), _mosiPin(mosiPin), _mountFreqHz(mountFreqHz), _remountFreqHz(remountFreqHz) {}

bool SDCardService::begin() {
  SPI.begin(_sckPin, _misoPin, _mosiPin, _csPin);
  if (!SD.begin(_csPin, SPI, _mountFreqHz)) {
    _mounted = false;
    return false;
  }

  _mounted = true;
  ensurePhotosDir();
  return true;
}

bool SDCardService::ensurePhotosDir() {
  if (!SD.exists("/PHOTOS")) {
    return SD.mkdir("/PHOTOS");
  }
  return true;
}

bool SDCardService::ensureMounted() {
  Serial.printf("ensureSDMounted: Checking SD status... Free heap: %d bytes\n", ESP.getFreeHeap());

  const uint8_t cardType = SD.cardType();
  if (cardType != CARD_NONE && _mounted) {
    Serial.println("ensureSDMounted: SD card still mounted OK");
    return true;
  }

  Serial.println("ensureSDMounted: SD card needs remounting...");

  SD.end();
  delay(100);

  SPI.end();
  delay(50);
  SPI.begin(_sckPin, _misoPin, _mosiPin, _csPin);
  delay(50);

  if (!SD.begin(_csPin, SPI, _remountFreqHz)) {
    Serial.println("ensureSDMounted: FAILED to remount SD card!");
    _mounted = false;
    return false;
  }

  Serial.println("ensureSDMounted: SD card successfully remounted!");
  _mounted = true;

  ensurePhotosDir();
  return true;
}
