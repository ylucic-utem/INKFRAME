#include "ui/DisplayUI.h"

#include <algorithm>

#include <M5Unified.h>

#include "services/PowerService.h"
#include "config.h"

namespace DisplayUI {

// ============ Static State Variables ============
static WifiState currentWifiState = WifiState::Disconnected;
static int32_t lastBatteryPercent = -1;
static uint32_t lastBatteryUpdateMs = 0;
static String currentStatusText = "";
static bool photoInfoVisible = false;
static bool progressVisible = false;
static int lastProgressPercent = -1;
static bool idleWarningVisible = false;
static uint32_t lastDisplayedSeconds = 0;
static bool offlineIndicatorVisible = false;
static bool renderingAnimationVisible = false;

// ============ Helper Functions ============
static Rect makeRect(int32_t x, int32_t y, int32_t w, int32_t h) {
  return Rect{x, y, w, h};
}

Rect taskbarRect() {
  const int32_t barH = taskbarHeight();
  return makeRect(0, M5.Display.height() - barH, M5.Display.width(), barH);
}

Rect imageRect() {
  const int32_t barH = taskbarHeight();
  return makeRect(0, 0, M5.Display.width(), M5.Display.height() - barH);
}

Rect bannerRect() {
  // Centered inside the image area.
  const Rect img = imageRect();
  const int32_t w = std::min<int32_t>(img.w - 40, 520);
  const int32_t h = 90;
  const int32_t x = img.x + (img.w - w) / 2;
  const int32_t y = img.y + (img.h - h) / 2;
  return makeRect(x, y, w, h);
}

void refreshRect(const Rect& r) {
  // Region-based refresh (partial update) when supported.
  M5.Display.display(r.x, r.y, r.w, r.h);
}

int32_t taskbarHeight() {
  return 60;
}

// ============ Button Layout ============
// Layout: [SLEEP] [PREV] ---- [status/cache] [wifi] [battery] ---- [QUAL] [NEXT]

Rect sleepButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 70;
  const int32_t buttonH = 40;
  const int32_t x = 10;
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect previousButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 70;
  const int32_t buttonH = 40;
  const int32_t x = sleepButtonRect().x + sleepButtonRect().w + 5;
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect nextButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 70;
  const int32_t buttonH = 40;
  const int32_t x = M5.Display.width() - (buttonW + 10);
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect qualityButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 80;
  const int32_t buttonH = 40;
  const int32_t x = nextButtonRect().x - (buttonW + 5);
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect infoButtonRect() {
  // Info button is hidden/virtual - touch on image area with long press
  // Return empty rect since it's not a visible button
  return makeRect(0, 0, 0, 0);
}

// ============ Basic Display Functions ============
void showSingleLineStatus(const String& line1) {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setCursor(10, 10);
  M5.Display.println(line1);
  M5.Display.flush();
}

void showTwoLineStatus(const String& line1, const String& line2) {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setCursor(10, 10);
  M5.Display.println(line1);
  M5.Display.setCursor(10, 40);
  M5.Display.println(line2);
  M5.Display.flush();
}

static Rect centeredTextRect(const String& text, int32_t cx, int32_t cy, int32_t padX, int32_t padY) {
  const int32_t textW = M5.Display.textWidth(text);
  const int32_t textH = M5.Display.fontHeight();

  const int32_t w = textW + padX * 2;
  const int32_t h = textH + padY * 2;
  const int32_t x = cx - (w / 2);
  const int32_t y = cy - (h / 2);
  return makeRect(x, y, w, h);
}

// ============ Splash Screen ============
void showSplash(const String& appName) {
  M5.Display.fillScreen(TFT_WHITE);

  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();

  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextDatum(datum_t::middle_center);

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.drawString(appName, cx, cy);

  M5.Display.flush();

  M5.Display.setTextDatum(prevDatum);
  M5.Display.setFont(prevFont);
}

void showSplashSimple(const String& appName) {
  M5.Display.fillScreen(TFT_WHITE);
  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();
  M5.Display.setTextColor(TFT_BLACK);
  switch (SPLASH_FONT) {
    case 0: M5.Display.setFont(&fonts::Font0); break;
    case 2: M5.Display.setFont(&fonts::Font2); break;
    case 4: M5.Display.setFont(&fonts::Font4); break;
    case 6: M5.Display.setFont(&fonts::Font6); break;
    case 7: M5.Display.setFont(&fonts::Font7); break;
    case 8: M5.Display.setFont(&fonts::Font8); break;
    default: M5.Display.setFont(&fonts::Font2); break;
  }
  M5.Display.setTextDatum(datum_t::middle_center);
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.drawString(appName, cx, cy);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  M5.Display.setTextDatum(prevDatum);
  M5.Display.setFont(prevFont);
}

void showSplashAnimated(const String& appName, uint32_t perCharDelayMs, uint32_t endHoldMs) {
  if (appName.length() == 0) {
    showSplash(appName);
    return;
  }

  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();

  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextDatum(datum_t::middle_center);

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;

  const Rect r = centeredTextRect(appName, cx, cy, 18, 14);

  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  refreshRect(r);

  for (size_t i = 1; i <= appName.length(); ++i) {
    const String partial = appName.substring(0, static_cast<uint16_t>(i));
    M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
    M5.Display.drawString(partial, cx, cy);
    refreshRect(r);
    if (perCharDelayMs > 0) {
      delay(perCharDelayMs);
    }
  }

  if (endHoldMs > 0) {
    delay(endHoldMs);
  }

  M5.Display.setTextDatum(prevDatum);
  M5.Display.setFont(prevFont);
}

// ============ Battery Icon (20x10 pixels) ============
void drawBatteryIcon(int32_t x, int32_t y, int percentage, bool charging) {
  const int32_t w = 20;
  const int32_t h = 10;
  const int32_t capW = 2;
  const int32_t capH = 4;
  
  // Clamp percentage
  percentage = std::max(0, std::min(100, percentage));
  
  // Clear area
  M5.Display.fillRect(x, y, w + capW + 2, h + 2, TFT_WHITE);
  
  // Draw battery outline
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  
  // Draw battery cap (positive terminal)
  M5.Display.fillRect(x + w, y + (h - capH) / 2, capW, capH, TFT_BLACK);
  
  // Calculate fill width
  const int32_t fillMaxW = w - 4;
  const int32_t fillW = (fillMaxW * percentage) / 100;
  
  // Choose fill pattern based on battery level
  if (percentage <= BATTERY_CRITICAL_PERCENT) {
    // Critical: hollow fill with exclamation
    M5.Display.drawRect(x + 2, y + 2, fillMaxW, h - 4, TFT_BLACK);
    // Draw ! inside
    M5.Display.fillRect(x + w/2 - 1, y + 2, 2, 3, TFT_BLACK);
    M5.Display.fillRect(x + w/2 - 1, y + 6, 2, 2, TFT_BLACK);
  } else if (percentage <= BATTERY_LOW_PERCENT) {
    // Low: striped fill
    for (int32_t i = 0; i < fillW; i += 2) {
      M5.Display.drawFastVLine(x + 2 + i, y + 2, h - 4, TFT_BLACK);
    }
  } else {
    // Normal: solid fill
    if (fillW > 0) {
      M5.Display.fillRect(x + 2, y + 2, fillW, h - 4, TFT_BLACK);
    }
  }
  
  // Charging indicator: small lightning bolt overlay
  if (charging) {
    // Simple arrow indicator
    M5.Display.fillTriangle(x + w/2 + 2, y + 1,
                            x + w/2 - 2, y + h/2,
                            x + w/2 + 1, y + h/2, TFT_WHITE);
    M5.Display.fillTriangle(x + w/2 - 2, y + h - 1,
                            x + w/2 + 2, y + h/2,
                            x + w/2 - 1, y + h/2, TFT_WHITE);
  }
}

bool updateBatteryIconIfNeeded() {
  const int32_t currentPercent = PowerService::batteryPercent();
  const uint32_t now = millis();
  
  // Check if update needed: significant change or timeout
  bool needsUpdate = false;
  
  if (lastBatteryPercent < 0) {
    needsUpdate = true;  // First time
  } else if (abs(currentPercent - lastBatteryPercent) >= BATTERY_CHANGE_THRESHOLD) {
    needsUpdate = true;  // Significant change
  } else if (now - lastBatteryUpdateMs >= BATTERY_REFRESH_INTERVAL_MS) {
    needsUpdate = true;  // Timeout
  }
  
  if (needsUpdate) {
    lastBatteryPercent = currentPercent;
    lastBatteryUpdateMs = now;
    return true;
  }
  
  return false;
}

// ============ WiFi Status Icon (12x10 pixels) ============
void drawWifiIcon(int32_t x, int32_t y, WifiState state) {
  const int32_t w = 12;
  const int32_t h = 10;
  
  // Clear area
  M5.Display.fillRect(x, y, w + 4, h + 2, TFT_WHITE);
  
  switch (state) {
    case WifiState::Connected:
      // Full WiFi symbol - three arcs
      M5.Display.drawArc(x + w/2, y + h, 8, 6, 225, 315, TFT_BLACK);
      M5.Display.drawArc(x + w/2, y + h, 5, 4, 225, 315, TFT_BLACK);
      M5.Display.fillCircle(x + w/2, y + h - 2, 2, TFT_BLACK);
      break;
      
    case WifiState::Connecting:
      // Animated/dotted arcs
      M5.Display.drawArc(x + w/2, y + h, 8, 7, 225, 270, TFT_BLACK);
      M5.Display.drawArc(x + w/2, y + h, 8, 7, 290, 315, TFT_BLACK);
      M5.Display.fillCircle(x + w/2, y + h - 2, 2, TFT_BLACK);
      break;
      
    case WifiState::Disconnected:
      // Outline only WiFi with strikethrough
      M5.Display.drawArc(x + w/2, y + h, 8, 7, 225, 315, TFT_BLACK);
      M5.Display.fillCircle(x + w/2, y + h - 2, 2, TFT_BLACK);
      // X through icon
      M5.Display.drawLine(x, y, x + w, y + h, TFT_BLACK);
      break;
      
    case WifiState::Error:
      // WiFi with X
      M5.Display.drawArc(x + w/2, y + h, 8, 7, 225, 315, TFT_BLACK);
      M5.Display.fillCircle(x + w/2, y + h - 2, 2, TFT_BLACK);
      // Bold X
      M5.Display.drawLine(x, y, x + w, y + h, TFT_BLACK);
      M5.Display.drawLine(x + w, y, x, y + h, TFT_BLACK);
      break;
  }
}

void setWifiState(WifiState state) {
  currentWifiState = state;
}

WifiState getWifiState() {
  return currentWifiState;
}

// ============ Progress Bar ============
void drawProgressBar(int32_t x, int32_t y, int32_t width, int32_t height, int percentage) {
  // Clear area
  M5.Display.fillRect(x - 2, y - 2, width + 4, height + 4, TFT_WHITE);
  
  // Draw outline
  M5.Display.drawRect(x, y, width, height, TFT_BLACK);
  
  if (percentage < 0) {
    // Indeterminate: draw animated pattern
    static int animOffset = 0;
    for (int32_t i = animOffset; i < width - 4; i += 8) {
      M5.Display.fillRect(x + 2 + i, y + 2, 4, height - 4, TFT_BLACK);
    }
    animOffset = (animOffset + 2) % 8;
  } else {
    // Determinate: fill based on percentage
    percentage = std::max(0, std::min(100, percentage));
    const int32_t fillW = ((width - 4) * percentage) / 100;
    if (fillW > 0) {
      M5.Display.fillRect(x + 2, y + 2, fillW, height - 4, TFT_BLACK);
    }
  }
}

void showProgress(const String& label, int percentage) {
  percentage = std::max(0, std::min(100, percentage));
  
  // Only redraw if percentage changed significantly
  if (progressVisible && abs(percentage - lastProgressPercent) < PROGRESS_UPDATE_THRESHOLD) {
    return;
  }
  lastProgressPercent = percentage;

  const Rect r = bannerRect();
  const int32_t barPadding = 20;
  const int32_t barHeight = 20;
  const int32_t barWidth = 400;  // 400px wide as specified
  const int32_t labelHeight = 24;
  const int32_t totalHeight = labelHeight + barHeight + 10;
  
  // Use fast EPD mode for partial updates
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  
  // Clear and draw background
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  
  // Draw label centered
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  const auto prevDatum = M5.Display.getTextDatum();
  M5.Display.setTextDatum(datum_t::top_center);
  
  const int32_t cx = r.x + r.w / 2;
  const int32_t labelY = r.y + (r.h - totalHeight) / 2;
  M5.Display.drawString(label, cx, labelY);
  
  // Draw progress bar (400px wide, 20px tall, centered)
  const int32_t barX = r.x + (r.w - barWidth) / 2;
  const int32_t barY = labelY + labelHeight + 5;
  
  drawProgressBar(barX, barY, barWidth, barHeight, percentage);
  
  // Draw percentage text at right side of bar
  String pctText = String(percentage) + "%";
  M5.Display.setTextDatum(datum_t::middle_right);
  M5.Display.drawString(pctText, barX + barWidth - 5, barY + barHeight / 2);
  
  M5.Display.setTextDatum(prevDatum);
  
  // Only do full refresh on first show
  if (!progressVisible) {
    refreshRect(r);
    progressVisible = true;
  }
}

void clearProgress() {
  if (!progressVisible) return;
  
  const Rect r = bannerRect();
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  refreshRect(r);
  
  progressVisible = false;
  lastProgressPercent = -1;
}

// ============ Photo Info Overlay ============
void showPhotoInfo(const PhotoInfo& photo, uint32_t downloadTimestamp, 
                   int32_t imgWidth, int32_t imgHeight) {
  const Rect img = imageRect();
  
  // Overlay in bottom quarter of screen
  const int32_t overlayH = img.h / 4;
  const int32_t overlayY = img.y + img.h - overlayH;
  const int32_t overlayX = img.x + 20;
  const int32_t overlayW = img.w - 40;
  const int32_t padding = 10;
  
  // Use fastest EPD mode for overlay
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  
  // Draw semi-transparent effect using white background with border
  M5.Display.fillRect(overlayX, overlayY, overlayW, overlayH, TFT_WHITE);
  M5.Display.drawRect(overlayX, overlayY, overlayW, overlayH, TFT_BLACK);
  M5.Display.drawRect(overlayX + 1, overlayY + 1, overlayW - 2, overlayH - 2, TFT_BLACK);
  
  // Set font for info text (size 3-4 as specified)
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  
  int32_t textY = overlayY + padding;
  const int32_t lineH = 20;
  
  // Title (bold/larger if possible)
  if (photo.title.length() > 0) {
    M5.Display.setFont(&fonts::Font4);
    String title = photo.title;
    if (title.length() > 50) title = title.substring(0, 47) + "...";
    M5.Display.setCursor(overlayX + padding, textY);
    M5.Display.print(title);
    textY += lineH + 5;
  }
  
  // Description
  M5.Display.setFont(&fonts::Font2);
  if (photo.description.length() > 0) {
    String desc = photo.description;
    if (desc.length() > 80) desc = desc.substring(0, 77) + "...";
    M5.Display.setCursor(overlayX + padding, textY);
    M5.Display.print(desc);
    textY += lineH;
  }
  
  // Source/URL (abbreviated)
  if (photo.url.length() > 0) {
    String source = "Source: ";
    // Extract domain from URL
    int start = photo.url.indexOf("://");
    if (start > 0) {
      int end = photo.url.indexOf("/", start + 3);
      if (end > 0) {
        source += photo.url.substring(start + 3, end);
      } else {
        source += photo.url.substring(start + 3);
      }
    }
    M5.Display.setCursor(overlayX + padding, textY);
    M5.Display.print(source);
    textY += lineH;
  }
  
  // Timestamp
  if (downloadTimestamp > 0) {
    String timeStr = "Downloaded: ";
    uint32_t mins = downloadTimestamp / 60000;
    if (mins < 60) {
      timeStr += String(mins) + " min ago";
    } else {
      timeStr += String(mins / 60) + " hrs ago";
    }
    M5.Display.setCursor(overlayX + padding, textY);
    M5.Display.print(timeStr);
    textY += lineH;
  }
  
  // Resolution if available
  if (imgWidth > 0 && imgHeight > 0) {
    String res = "Resolution: " + String(imgWidth) + " x " + String(imgHeight);
    M5.Display.setCursor(overlayX + padding, textY);
    M5.Display.print(res);
  }
  
  // Refresh the overlay region
  Rect overlayRect = makeRect(overlayX, overlayY, overlayW, overlayH);
  refreshRect(overlayRect);
  
  photoInfoVisible = true;
}

void hidePhotoInfo() {
  if (!photoInfoVisible) return;
  
  // Just mark as hidden - the next photo render will clear it
  photoInfoVisible = false;
  
  // Option: Could trigger a partial refresh of the overlay area
  // but this would require storing/restoring the underlying image
}

bool isPhotoInfoVisible() {
  return photoInfoVisible;
}

// ============ Button Feedback ============
void showButtonFeedback(const Rect& buttonRect) {
  // Invert the button area briefly
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  
  // Invert colors in button area
  M5.Display.fillRect(buttonRect.x + 2, buttonRect.y + 2, 
                      buttonRect.w - 4, buttonRect.h - 4, TFT_BLACK);
  refreshRect(buttonRect);
  
  delay(BUTTON_FEEDBACK_MS);
  
  // Restore (will be redrawn by caller or next update)
}

void showTouchFeedback() {
  // Quick corner flash for immediate feedback
  const int32_t size = 20;
  const Rect corner = makeRect(M5.Display.width() - size - 5, 5, size, size);
  
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  M5.Display.fillRect(corner.x, corner.y, corner.w, corner.h, TFT_BLACK);
  refreshRect(corner);
  
  delay(50);
  
  M5.Display.fillRect(corner.x, corner.y, corner.w, corner.h, TFT_WHITE);
  refreshRect(corner);
}

// ============ Status Text ============
void setStatusText(const String& status) {
  if (status == currentStatusText) return;
  
  currentStatusText = status;
  
  // Status text region in middle of taskbar
  const Rect bar = taskbarRect();
  const int32_t statusX = previousButtonRect().x + previousButtonRect().w + 10;
  const int32_t statusY = bar.y + 20;
  const int32_t statusW = qualityButtonRect().x - statusX - 10;
  const int32_t statusH = 20;
  
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  M5.Display.fillRect(statusX, statusY, statusW, statusH, TFT_WHITE);
  
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(statusX, statusY);
  
  // Truncate if needed
  String text = status;
  if (text.length() > 20) text = text.substring(0, 17) + "...";
  M5.Display.print(text);
  
  Rect statusRect = makeRect(statusX, statusY, statusW, statusH);
  refreshRect(statusRect);
}

void clearStatusText() {
  if (currentStatusText.length() == 0) return;
  currentStatusText = "";
  
  const Rect bar = taskbarRect();
  const int32_t statusX = previousButtonRect().x + previousButtonRect().w + 10;
  const int32_t statusY = bar.y + 20;
  const int32_t statusW = qualityButtonRect().x - statusX - 10;
  const int32_t statusH = 20;
  
  M5.Display.fillRect(statusX, statusY, statusW, statusH, TFT_WHITE);
  Rect statusRect = makeRect(statusX, statusY, statusW, statusH);
  refreshRect(statusRect);
}

// ============ Rendering Animation ============
void showRenderingAnimation(int step) {
  const Rect r = bannerRect();
  
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(datum_t::middle_center);
  
  // Cycle through dots: "Rendering.", "Rendering..", "Rendering..."
  String text = "Rendering";
  int dots = (step % 3) + 1;
  for (int i = 0; i < dots; i++) {
    text += ".";
  }
  
  M5.Display.drawString(text, r.x + r.w / 2, r.y + r.h / 2);
  
  if (!renderingAnimationVisible) {
    refreshRect(r);
    renderingAnimationVisible = true;
  }
}

void hideRenderingAnimation() {
  if (!renderingAnimationVisible) return;
  
  const Rect r = bannerRect();
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  refreshRect(r);
  
  renderingAnimationVisible = false;
}

// ============ Cache Position Visualization ============
void drawCachePositionIndicator(uint8_t position, uint8_t total) {
  if (total == 0) return;
  
  const Rect img = imageRect();
  const int32_t dotRadius = 4;
  const int32_t dotSpacing = 12;
  const int32_t maxDots = std::min((int)total, 10);  // Cap at 10 dots
  
  const int32_t totalW = maxDots * dotSpacing;
  const int32_t startX = img.x + (img.w - totalW) / 2;
  const int32_t y = img.y + img.h - 20;  // 20px from bottom of image area
  
  // Clear area
  M5.Display.fillRect(startX - dotRadius, y - dotRadius, 
                      totalW + dotRadius * 2, dotRadius * 2 + 2, TFT_WHITE);
  
  // Draw dots
  for (uint8_t i = 0; i < maxDots; i++) {
    const int32_t dotX = startX + i * dotSpacing + dotSpacing / 2;
    
    if (i == position % maxDots) {
      // Current position: filled dot
      M5.Display.fillCircle(dotX, y, dotRadius, TFT_BLACK);
    } else {
      // Other positions: hollow dot
      M5.Display.drawCircle(dotX, y, dotRadius, TFT_BLACK);
    }
  }
  
  // Refresh
  Rect indicatorRect = makeRect(startX - dotRadius, y - dotRadius - 2, 
                                 totalW + dotRadius * 2, dotRadius * 2 + 4);
  refreshRect(indicatorRect);
}

// ============ Error Banner ============
void showErrorBanner(const String& title, const String& message, ErrorType type) {
  const Rect r = bannerRect();
  
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  
  // Double border for emphasis
  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  M5.Display.drawRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, TFT_BLACK);
  
  // Draw type-specific icon
  const int32_t iconX = r.x + 15;
  const int32_t iconY = r.y + r.h / 2;
  const int32_t iconSize = 24;
  
  switch (type) {
    case ErrorType::Network:
      // WiFi with X
      drawWifiIcon(iconX, iconY - iconSize/2, WifiState::Error);
      break;
      
    case ErrorType::SDCard:
      // SD card icon
      M5.Display.drawRect(iconX, iconY - iconSize/2, iconSize - 4, iconSize, TFT_BLACK);
      M5.Display.fillTriangle(iconX + iconSize - 4, iconY - iconSize/2,
                              iconX + iconSize - 4, iconY - iconSize/2 + 6,
                              iconX + iconSize - 10, iconY - iconSize/2, TFT_BLACK);
      break;
      
    case ErrorType::API:
      // Cloud with X
      M5.Display.drawCircle(iconX + iconSize/2, iconY, iconSize/3, TFT_BLACK);
      M5.Display.drawLine(iconX + 4, iconY - 4, iconX + iconSize - 4, iconY + 4, TFT_BLACK);
      break;
      
    case ErrorType::Render:
      // Image icon with X
      M5.Display.drawRect(iconX, iconY - iconSize/2, iconSize, iconSize, TFT_BLACK);
      M5.Display.drawLine(iconX, iconY - iconSize/2, iconX + iconSize, iconY + iconSize/2, TFT_BLACK);
      break;
      
    case ErrorType::Generic:
    default:
      // Exclamation mark
      M5.Display.fillRect(iconX + iconSize/2 - 2, iconY - 10, 4, 14, TFT_BLACK);
      M5.Display.fillRect(iconX + iconSize/2 - 2, iconY + 6, 4, 4, TFT_BLACK);
      break;
  }
  
  // Draw text
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  
  const int32_t textX = iconX + iconSize + 15;
  const int32_t titleY = r.y + r.h / 2 - 12;
  const int32_t msgY = r.y + r.h / 2 + 8;
  
  M5.Display.setCursor(textX, titleY);
  M5.Display.print(title);
  
  M5.Display.setCursor(textX, msgY);
  M5.Display.print(message);
  
  refreshRect(r);
}

// ============ Button Drawing ============
static void drawButton(const Rect& r, const char* label, bool inverted = false) {
  const uint32_t fg = inverted ? TFT_WHITE : TFT_BLACK;
  const uint32_t bg = inverted ? TFT_BLACK : TFT_WHITE;

  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  M5.Display.fillRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, bg);

  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(fg);

  const int32_t labelX = r.x + 8;
  const int32_t labelY = r.y + 12;
  M5.Display.setCursor(labelX, labelY);
  M5.Display.print(label);
}

// ============ Banner Functions ============
static void drawBannerImpl(const String& line1, const String& line2, bool hasLine2) {
  const Rect r = bannerRect();

  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);

  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);

  const auto prevDatum = M5.Display.getTextDatum();
  M5.Display.setTextDatum(datum_t::middle_center);

  const int32_t cx = r.x + r.w / 2;
  if (hasLine2) {
    M5.Display.drawString(line1, cx, r.y + r.h / 2 - 14);
    M5.Display.drawString(line2, cx, r.y + r.h / 2 + 14);
  } else {
    M5.Display.drawString(line1, cx, r.y + r.h / 2);
  }

  M5.Display.setTextDatum(prevDatum);
  refreshRect(r);
}

void showBanner(const String& line1) {
  drawBannerImpl(line1, String(), false);
}

void showBanner(const String& line1, const String& line2) {
  drawBannerImpl(line1, line2, true);
}

void clearBanner() {
  const Rect r = bannerRect();
  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  refreshRect(r);
}

// ============ Taskbar Functions ============
// Basic taskbar (backward compatible)
void drawTaskbar(const String& title, bool qualityEnabled) {
  drawTaskbar(title, qualityEnabled, 0, 0, false);
}

// Extended taskbar with cache position and offline mode
void drawTaskbar(const String& title, bool qualityEnabled, 
                 uint8_t cachePosition, uint8_t cacheCount,
                 bool offlineMode) {
  // Use current battery and wifi state
  WifiState wifiState = offlineMode ? WifiState::Disconnected : currentWifiState;
  int battPercent = PowerService::batteryPercent();
  
  drawTaskbarFull(title, qualityEnabled, cachePosition, cacheCount,
                  offlineMode, wifiState, battPercent, false);
}

// Full-featured taskbar with all status indicators
void drawTaskbarFull(const String& title, bool qualityEnabled,
                     uint8_t cachePosition, uint8_t cacheCount,
                     bool offlineMode, WifiState wifiState,
                     int batteryPercent, bool batteryCharging) {
  const int32_t barH = taskbarHeight();
  const int32_t y0 = M5.Display.height() - barH;

  // Bar background (white) + separator line.
  M5.Display.fillRect(0, y0, M5.Display.width(), barH, TFT_WHITE);
  M5.Display.drawFastHLine(0, y0, M5.Display.width(), TFT_BLACK);

  // Buttons
  drawButton(sleepButtonRect(), "SLEEP");
  drawButton(previousButtonRect(), "PREV");
  drawButton(qualityButtonRect(), "QUAL", qualityEnabled);
  drawButton(nextButtonRect(), "NEXT");

  // Calculate middle section bounds
  const int32_t leftEnd = previousButtonRect().x + previousButtonRect().w + 8;
  const int32_t rightStart = qualityButtonRect().x - 8;
  const int32_t middleW = rightStart - leftEnd;
  
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_BLACK);

  // Right side: Battery + WiFi icons
  const int32_t iconY = y0 + 8;
  int32_t iconX = rightStart - 25;  // Start from right
  
  // Battery icon
  drawBatteryIcon(iconX, iconY, batteryPercent, batteryCharging);
  iconX -= 18;  // Move left for WiFi
  
  // WiFi icon
  drawWifiIcon(iconX, iconY, wifiState);
  
  // Battery percentage text below icon
  if (batteryPercent >= 0) {
    String battText = String(batteryPercent) + "%";
    M5.Display.setCursor(rightStart - 28, y0 + 22);
    M5.Display.print(battText);
  }

  // Left side of middle: Cache info and title
  int32_t textX = leftEnd;
  int32_t textY = y0 + 8;
  
  // Cache position indicator
  if (cacheCount > 0) {
    String cacheText = "[" + String(cachePosition + 1) + "/" + String(cacheCount) + "]";
    M5.Display.setCursor(textX, textY);
    M5.Display.print(cacheText);
    textX += M5.Display.textWidth(cacheText) + 8;
  }
  
  // Offline indicator
  if (offlineMode) {
    M5.Display.setCursor(textX, textY);
    M5.Display.print("OFFLINE");
    textX += 50;
  }

  // Title (on second line if we have cache info)
  String t = title;
  if (t.length() > 0) {
    const int maxChars = 35;
    if (t.length() > maxChars) t = t.substring(0, maxChars - 3) + "...";
    
    const int32_t titleY = (cacheCount > 0 || offlineMode) ? y0 + 24 : y0 + 16;
    M5.Display.setCursor(leftEnd, titleY);
    M5.Display.print(t);
  }
  
  // Status text (if set)
  if (currentStatusText.length() > 0) {
    const int32_t statusY = y0 + 40;
    M5.Display.setCursor(leftEnd, statusY);
    M5.Display.print(currentStatusText);
  }
}

// ============ Idle Warning ============
void showIdleWarning(uint32_t secondsRemaining) {
  if (secondsRemaining == 0) {
    clearIdleWarning();
    return;
  }
  
  if (secondsRemaining == lastDisplayedSeconds && idleWarningVisible) {
    return;
  }
  lastDisplayedSeconds = secondsRemaining;
  
  const Rect bar = taskbarRect();
  const int32_t warningX = previousButtonRect().x + previousButtonRect().w + 5;
  const int32_t warningY = bar.y + 10;
  const int32_t warningW = 130;
  const int32_t warningH = 16;
  
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  
  String warning = String("Zzz ") + secondsRemaining + "s";
  
  M5.Display.fillRect(warningX, warningY, warningW, warningH, TFT_WHITE);
  M5.Display.setCursor(warningX, warningY);
  M5.Display.print(warning);
  
  if (!idleWarningVisible) {
    Rect warnRect = makeRect(warningX, warningY, warningW, warningH);
    refreshRect(warnRect);
    idleWarningVisible = true;
  }
}

void clearIdleWarning() {
  if (!idleWarningVisible) return;
  
  const Rect bar = taskbarRect();
  const int32_t warningX = previousButtonRect().x + previousButtonRect().w + 5;
  const int32_t warningY = bar.y + 10;
  const int32_t warningW = 130;
  const int32_t warningH = 16;
  
  M5.Display.fillRect(warningX, warningY, warningW, warningH, TFT_WHITE);
  Rect warnRect = makeRect(warningX, warningY, warningW, warningH);
  refreshRect(warnRect);
  
  idleWarningVisible = false;
  lastDisplayedSeconds = 0;
}

// ============ Offline Indicator ============
void showOfflineIndicator(bool show) {
  if (show == offlineIndicatorVisible) return;
  
  const Rect bar = taskbarRect();
  const int32_t indicatorX = previousButtonRect().x + previousButtonRect().w + 80;
  const int32_t indicatorY = bar.y + 8;
  const int32_t indicatorW = 70;
  const int32_t indicatorH = 14;
  
  if (show) {
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.fillRect(indicatorX, indicatorY, indicatorW, indicatorH, TFT_WHITE);
    M5.Display.drawRect(indicatorX, indicatorY, indicatorW, indicatorH, TFT_BLACK);
    M5.Display.setCursor(indicatorX + 4, indicatorY + 2);
    M5.Display.print("OFFLINE");
  } else {
    M5.Display.fillRect(indicatorX, indicatorY, indicatorW, indicatorH, TFT_WHITE);
  }
  
  Rect indicatorRect = makeRect(indicatorX, indicatorY, indicatorW, indicatorH);
  refreshRect(indicatorRect);
  
  offlineIndicatorVisible = show;
}

} // namespace DisplayUI
