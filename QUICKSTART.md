# Quick Start Guide - M5Stack PaperS3 Photo Viewer

## Prerequisites
- M5Stack PaperS3 device
- USB-C cable
- VS Code with PlatformIO installed
- WiFi network access
- Optional: SD card for photo storage

## Step 1: Configure WiFi

Copy `include/config.local.h.example` to `include/config.local.h` and set your credentials:

```cpp
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

`config.local.h` is git-ignored and loaded by `include/config.h`.


## Step 2: Build the Project

Open the terminal in VS Code and run:

```bash
# Build for M5Stack PaperS3
pio run -e PaperS3
```

## Step 3: Upload to Device

1. Connect M5Stack PaperS3 via USB-C to your computer
2. Run:

```bash
pio run -e PaperS3 -t upload
```

3. Wait for the upload to complete (usually 30-60 seconds)

## Step 4: Monitor Serial Output

```bash
pio run -e PaperS3 -t monitor
```

You should see:
- WiFi connection status
- API requests being made
- Photo data being parsed
- SD card operations (if enabled)

## Step 5: Use the Device

1. Device will automatically display the first photo after connecting to WiFi
2. Press the **Right Button (Button A)** to load the next random photo
3. Alternatively, **touch the right side of the screen** to load the next photo
4. Photos and metadata are automatically saved to the SD card

## Troubleshooting

### WiFi won't connect
- Verify SSID and password are correct (case-sensitive)
- Ensure WiFi is 2.4GHz (not 5GHz)
- Check WiFi range and signal strength
- Restart the M5Stack device

### No photos loading
- Check internet connection
- Verify API is accessible: https://boringapi.com/api/v1/photos/random?num=1
- Check serial monitor for error messages
- Review firewall settings

### SD card not working
- Ensure SD card is properly inserted
- Try formatting the SD card
- Check if SD slot has physical issues

### Display issues
- Ensure M5Unified library is properly installed
- Try clearing the serial monitor and restarting
- Check if display connector is loose

## Using the Advanced Version

For more features, replace the main.cpp content with code from `src/main_advanced.cpp`:

1. Rename `src/main.cpp` to `src/main_basic.cpp`
2. Rename `src/main_advanced.cpp` to `src/main.cpp`
3. Configure `include/config.h` with your preferences
4. Build and upload

## Configuration Options

Edit `include/config.h` to customize:

```cpp
// WiFi
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// API
#define API_URL "https://boringapi.com/api/v1/photos/random?num=1"

// Features
#define SD_CARD_ENABLE true
#define BUTTON_ENABLE_TOUCH true
```

## Next Steps

- Explore the source code to understand the implementation
- Modify the API URL to use different photo sources
- Customize the display layout
- Add JPEG decoding for full image display
- Implement additional features like favorites, categories, etc.

## Support

For issues with:
- **M5Stack Hardware**: Visit https://docs.m5stack.com/
- **M5Unified Library**: Check https://github.com/m5stack/m5unified
- **ArduinoJson**: See https://arduinojson.org/
- **Boring API**: Visit https://boringapi.com/

## Additional Resources

- [M5Stack Official Documentation](https://docs.m5stack.com/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/)
