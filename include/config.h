/**
 * Configuration header for M5Stack PaperS3 Photo Viewer
 * Edit this file to customize behavior without modifying main.cpp
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============ WiFi Configuration ============
// IMPORTANT: Configure these with your network credentials
#define WIFI_SSID "D815"
#define WIFI_PASSWORD "inti815inti815"
#define WIFI_TIMEOUT_MS 20000  // WiFi connection timeout in milliseconds

// ============ API Configuration ============
#define PHOTO_API_URL "https://boringapi.com/api/v1/photos/random?num=1"
#define API_TIMEOUT_MS 15000   // HTTP request timeout in milliseconds (reduced from 30s for faster feedback)
#define PHOTO_BUFFER_SIZE 4096 // JSON buffer size for parsing

// ============ Network Retry Configuration ============
#define API_RETRY_COUNT 3              // Number of retry attempts for failed requests
#define API_RETRY_BASE_DELAY_MS 1000   // Initial retry delay in milliseconds (exponential backoff)
#define API_CONNECTION_TIMEOUT_MS 3000 // TCP connection timeout (fail fast if unreachable)

// ============ Request Prioritization ============
#define MAX_PENDING_REQUESTS 2         // Skip intermediate requests if queue exceeds this

// ============ Display Configuration ============
#define DISPLAY_REFRESH_DELAY 100  // Delay between display updates (ms)
#define DISPLAY_ENABLE_FLUSH true  // Enable display flush after update

// ============ App / Splash Configuration ============
// Name shown on the boot splash screen.
#ifndef APP_NAME
#define APP_NAME "InkFrame"
#endif

// Total splash time budget (roughly). The animation will pace characters to fit this.
// Recommended: 3000-5000ms.
#ifndef SPLASH_TOTAL_MS
#define SPLASH_TOTAL_MS 4500
#endif

// Extra hold after the full name is shown.
#ifndef SPLASH_END_HOLD_MS
#define SPLASH_END_HOLD_MS 600
#endif

// Splash font size index (M5Unified built-in fonts). Typical values: 2..7.
#ifndef SPLASH_FONT
#define SPLASH_FONT 7
#endif

// Splash mode: 0 = text (APP_NAME), 1 = bitmap (1-bit array), 2 = JPEG file from SD.
#ifndef SPLASH_MODE
#define SPLASH_MODE 1
#endif

// Bitmap splash: width, height (must match your bitmap array).
#ifndef SPLASH_BITMAP_WIDTH
#define SPLASH_BITMAP_WIDTH 960
#endif
#ifndef SPLASH_BITMAP_HEIGHT
#define SPLASH_BITMAP_HEIGHT 540
#endif

// JPEG splash: path on SD card (e.g., "/splash.jpg"). Must be 960x540 or will be centered/cropped.
#ifndef SPLASH_JPEG_PATH
#define SPLASH_JPEG_PATH "/name.jpg"
#endif

// JPEG splash URL: download from this URL on first boot if file doesn't exist on SD.
#ifndef SPLASH_JPEG_URL
#define SPLASH_JPEG_URL "https://i.ibb.co/v4SgGhGg/name.jpg"
#endif

// ============ Button Configuration ============
#define BUTTON_DEBOUNCE_MS 500      // Debounce interval for button press
#define BUTTON_ENABLE_TOUCH true    // Enable touch input in addition to button
#define TOUCH_THRESHOLD_X_RATIO 0.66 // X threshold for touch (2/3 from left)

// ============ SD Card Configuration ============
#define SD_CARD_ENABLE true              // Enable SD card operations
#define SD_CARD_CS_PIN GPIO_NUM_21       // Chip select pin for SD card
#define SD_SAVE_PHOTOS true              // Save downloaded photos
#define SD_SAVE_METADATA true            // Save photo metadata to text files
#define SD_PHOTO_FILENAME "/photo.jpg"   // Path for the current photo

// ============ Serial/Debug Configuration ============
#define SERIAL_BAUD_RATE 115200
#define SERIAL_DEBUG true  // Enable debug messages

// ============ Memory Configuration ============
#define HTTP_BUFFER_SIZE 2048  // Buffer size for HTTP downloads
#define JSON_DOCUMENT_SIZE 4096 // Size for StaticJsonDocument

// ============ Power Management Configuration ============
// Idle timeout before entering deep sleep (10 minutes = 600,000 ms)
#define IDLE_TIMEOUT_MS 600000

// Deep sleep wake interval for automatic updates (12 hours = 43200 seconds)
#define DEEP_SLEEP_WAKE_INTERVAL_SECONDS 43200

// Show splash screen before entering sleep
#define SHOW_SPLASH_ON_SLEEP true

// Disconnect WiFi when approaching idle (saves power before full sleep)
#define DISCONNECT_WIFI_ON_IDLE true

// WiFi idle disconnect threshold (2 minutes before going idle)
#define WIFI_IDLE_DISCONNECT_MS 120000

// Idle warning time (seconds before sleep to show warning)
#define IDLE_WARNING_SECONDS 30

// Critical battery percentage threshold
#define BATTERY_CRITICAL_PERCENT 15

// Extended wake interval for low battery (48 hours = 172800 seconds)
#define LOW_BATTERY_WAKE_INTERVAL_SECONDS 172800

#endif // CONFIG_H
