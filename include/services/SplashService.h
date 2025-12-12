#pragma once

#include <Arduino.h>

namespace SplashService {

// Performs an explicit full refresh to a white background, then plays the splash animation.
// This is intended to run once on boot.
void showBootSplash(const String& appName);

} // namespace SplashService
