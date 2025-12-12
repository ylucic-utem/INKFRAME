#include "ui/DisplayUI.h"

#include <algorithm>

#include <M5Unified.h>

#include "services/PowerService.h"
#include "config.h"

namespace DisplayUI {

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
  // On M5GFX, display(x,y,w,h) updates only that region for EPD panels.
  M5.Display.display(r.x, r.y, r.w, r.h);
}

int32_t taskbarHeight() {
  return 60;
}

Rect nextButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 110;
  const int32_t buttonH = 40;
  const int32_t x = M5.Display.width() - (buttonW + 10);
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect qualityButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 120;
  const int32_t buttonH = 40;
  const int32_t x = nextButtonRect().x - (buttonW + 10);
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

Rect sleepButtonRect() {
  const int32_t barH = taskbarHeight();
  const int32_t buttonW = 110;
  const int32_t buttonH = 40;
  const int32_t x = 10;
  const int32_t y = M5.Display.height() - barH + (barH - buttonH) / 2;
  return makeRect(x, y, buttonW, buttonH);
}

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

void showSplash(const String& appName) {
  // Full-screen white background, centered large text.
  M5.Display.fillScreen(TFT_WHITE);

  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();

  M5.Display.setTextColor(TFT_BLACK);
  // Large font for a clean, modern look.
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextDatum(datum_t::middle_center);

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.drawString(appName, cx, cy);

  // One full flush here gives a clean, stable base.
  M5.Display.flush();

  M5.Display.setTextDatum(prevDatum);
  M5.Display.setFont(prevFont);
}

void showSplashSimple(const String& appName) {
  // Full-screen white with big centered name and a full-frame refresh.
  M5.Display.fillScreen(TFT_WHITE);
  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();
  M5.Display.setTextColor(TFT_BLACK);
  // Map SPLASH_FONT index to an available font pointer.
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
  // Push the whole frame to ensure visibility on EPD.
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  M5.Display.setTextDatum(prevDatum);
  M5.Display.setFont(prevFont);
}

void showSplashAnimated(const String& appName, uint32_t perCharDelayMs, uint32_t endHoldMs) {
  if (appName.length() == 0) {
    showSplash(appName);
    return;
  }

  // Caller should have already established a clean background (typically a full refresh).
  // Here we only clear/refresh the text region to keep the animation fast.

  const auto prevDatum = M5.Display.getTextDatum();
  const auto prevFont = M5.Display.getFont();

  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextDatum(datum_t::middle_center);

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;

  // Refresh only the text area while animating.
  const Rect r = centeredTextRect(appName, cx, cy, 18, 14);

  // First paint: blank area so the first partial refresh is clean.
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

static void drawBannerImpl(const String& line1, const String& line2, bool hasLine2) {
  const Rect r = bannerRect();

  M5.Display.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);

  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);

  // Center text using datum. This keeps it stable regardless of string length.
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

static void drawButton(const Rect& r, const char* label, bool inverted = false) {
  const uint32_t fg = inverted ? TFT_WHITE : TFT_BLACK;
  const uint32_t bg = inverted ? TFT_BLACK : TFT_WHITE;

  M5.Display.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  M5.Display.fillRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, bg);

  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(fg);

  const int32_t labelX = r.x + 12;
  const int32_t labelY = r.y + 12;
  M5.Display.setCursor(labelX, labelY);
  M5.Display.print(label);
}

void drawTaskbar(const String& title, bool qualityEnabled) {
  const int32_t barH = taskbarHeight();
  const int32_t y0 = M5.Display.height() - barH;

  // Bar background (white) + separator line.
  M5.Display.fillRect(0, y0, M5.Display.width(), barH, TFT_WHITE);
  M5.Display.drawFastHLine(0, y0, M5.Display.width(), TFT_BLACK);

  // Buttons.
  drawButton(sleepButtonRect(), "SLEEP");
  drawButton(qualityButtonRect(), "QUALITY", qualityEnabled);
  drawButton(nextButtonRect(), "NEXT");

  // Battery + title in the middle area.
  const int batt = PowerService::batteryPercent();
  String battText = (batt >= 0) ? String("BAT ") + batt + "%" : String("BAT --");

  const int32_t left = sleepButtonRect().x + sleepButtonRect().w + 10;
  const int32_t right = qualityButtonRect().x - 10;
  const int32_t midW = right - left;
  const int32_t textY = y0 + 10;

  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_BLACK);

  // Battery at the left of the mid area.
  M5.Display.setCursor(left, textY);
  M5.Display.print(battText);

  // Title after battery (truncate to fit).
  String t = title;
  if (t.length() > 0) {
    // Simple truncation; keep UI stable across fonts.
    const int maxChars = 32;
    if (t.length() > maxChars) t = t.substring(0, maxChars) + "...";
    M5.Display.setCursor(left, textY + 18);
    M5.Display.print("Title: ");
    M5.Display.print(t);
  }
}

static bool idleWarningVisible = false;
static uint32_t lastDisplayedSeconds = 0;

void showIdleWarning(uint32_t secondsRemaining) {
  if (secondsRemaining == 0) {
    clearIdleWarning();
    return;
  }
  
  // Only update display when seconds change
  if (secondsRemaining == lastDisplayedSeconds && idleWarningVisible) {
    return;
  }
  lastDisplayedSeconds = secondsRemaining;
  
  // Draw warning in taskbar area
  const Rect bar = taskbarRect();
  const int32_t warningX = sleepButtonRect().x + sleepButtonRect().w + 5;
  const int32_t warningY = bar.y + 10;
  const int32_t warningW = 130;
  const int32_t warningH = 16;
  
  // Draw sleep countdown
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_BLACK);
  
  String warning = String("Zzz ") + secondsRemaining + "s";
  
  // Clear previous text
  M5.Display.fillRect(warningX, warningY, warningW, warningH, TFT_WHITE);
  M5.Display.setCursor(warningX, warningY);
  M5.Display.print(warning);
  
  // Only do a partial refresh when first showing the warning
  if (!idleWarningVisible) {
    Rect warnRect = makeRect(warningX, warningY, warningW, warningH);
    refreshRect(warnRect);
    idleWarningVisible = true;
  }
}

void clearIdleWarning() {
  if (!idleWarningVisible) return;
  
  const Rect bar = taskbarRect();
  const int32_t warningX = sleepButtonRect().x + sleepButtonRect().w + 5;
  const int32_t warningY = bar.y + 10;
  const int32_t warningW = 130;
  const int32_t warningH = 16;
  
  M5.Display.fillRect(warningX, warningY, warningW, warningH, TFT_WHITE);
  Rect warnRect = makeRect(warningX, warningY, warningW, warningH);
  refreshRect(warnRect);
  
  idleWarningVisible = false;
  lastDisplayedSeconds = 0;
}

static bool progressVisible = false;
static int lastProgressPercent = -1;

void showProgress(const String& label, int percentage) {
  // Clamp percentage
  percentage = std::max(0, std::min(100, percentage));
  
  // Only redraw if percentage changed significantly (by 5% or more) or first draw
  if (progressVisible && abs(percentage - lastProgressPercent) < 5) {
    return;
  }
  lastProgressPercent = percentage;

  // Progress bar dimensions - centered in the banner area
  const Rect r = bannerRect();
  const int32_t barPadding = 20;
  const int32_t barHeight = 16;
  const int32_t labelHeight = 24;
  const int32_t totalHeight = labelHeight + barHeight + 10;
  
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
  
  // Draw progress bar outline
  const int32_t barX = r.x + barPadding;
  const int32_t barY = labelY + labelHeight + 5;
  const int32_t barW = r.w - (barPadding * 2);
  
  M5.Display.drawRect(barX, barY, barW, barHeight, TFT_BLACK);
  
  // Draw filled portion (black fill for progress)
  const int32_t filledW = (barW - 4) * percentage / 100;
  if (filledW > 0) {
    M5.Display.fillRect(barX + 2, barY + 2, filledW, barHeight - 4, TFT_BLACK);
  }
  
  // Draw percentage text at right side of bar
  String pctText = String(percentage) + "%";
  M5.Display.setTextDatum(datum_t::middle_right);
  M5.Display.drawString(pctText, barX + barW - 5, barY + barHeight / 2);
  
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

} // namespace DisplayUI

