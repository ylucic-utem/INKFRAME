# M5Stack PaperS3 Photo Viewer

An Arduino program for the M5Stack PaperS3 device that displays random photos from the Boring API with the ability to navigate through photos and save them to the SD card.

## Features

- **WiFi Connectivity**: Connects to your WiFi network to fetch photos from the API
- **API Integration**: Fetches random photos from https://boringapi.com/api/v1/photos/random
- **JSON Parsing**: Parses API responses using ArduinoJson library
- **Display Support**: Shows photo title and description on the grayscale EPD display
- **Next Button**: Hardware button or touch input to load the next photo
- **SD Card Support**: Saves photo metadata and images to the M5Stack's SD card
- **Grayscale Optimization**: Utilizes the M5Stack PaperS3's E-Ink grayscale capabilities
- **Error Handling**: Displays helpful error messages on the display

## Hardware Requirements

- **M5Stack PaperS3** device
- USB-C cable for programming
- WiFi network access
- Optional: SD card for saving photos

## Software Setup

### 1. Install Dependencies

The following libraries are automatically installed via PlatformIO:
- **M5Unified** (v0.1.13+) - M5Stack hardware abstraction layer
- **ArduinoJson** (v7.0.4+) - JSON parsing library
- **ESP-IDF** - Espressif IoT Development Framework

### 2. Configure WiFi Credentials

Copy [`include/config.local.h.example`](include/config.local.h.example) to `include/config.local.h` and set your WiFi credentials:

```cpp
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

`config.local.h` is ignored by git and automatically loaded by [`include/config.h`](include/config.h).

### 3. Build and Upload

Using PlatformIO:

```bash
# Build the project
pio run -e PaperS3

# Upload to device
pio run -e PaperS3 -t upload

# Monitor serial output
pio run -e PaperS3 -t monitor
```


## Hardware Pin Notes

This project currently targets **M5Stack PaperS3** SD wiring. SD SPI pins are defined once in [`include/config.h`](include/config.h) (`SD_SPI_CS_PIN`, `SD_SPI_SCK_PIN`, `SD_SPI_MOSI_PIN`, `SD_SPI_MISO_PIN`) and consumed by `main.cpp`.

If you are porting to different hardware, update those values in `config.h`.

## Project Structure

```
EPD-image/
├── platformio.ini           # PlatformIO configuration
├── README.md               # This file
├── src/
│   └── main.cpp            # Main Arduino program
├── include/                # Header files (if needed)
├── lib/                    # Local libraries (if needed)
└── test/                   # Test files (if needed)
```

## How It Works

### Initialization
1. M5Unified is initialized
2. SD card is mounted (if available)
3. Device connects to WiFi
4. First photo is automatically fetched

### Main Loop
1. Checks for button press or touch input
2. When triggered, fetches a new random photo from the API
3. Parses the JSON response to extract photo URL, title, and description
4. Displays the information on the grayscale E-Ink screen
5. Saves photo metadata to SD card

### API Response Structure

The program expects responses in this format:

```json
{
  "success": true,
  "message": "Here are 1 random photos!",
  "photos": [
    {
      "file_size": 236,
      "title": "Photo Title",
      "description": "Photo description...",
      "height": 853,
      "width": 1280,
      "id": 146,
      "url": "https://boringapi.com/api/v1/static/photos/146.jpeg"
    }
  ]
}
```

## Button Controls

- **Button A (Right Button)**: Load next photo
- **Touch Input**: Touch the right 1/3 of the screen to load next photo (if touch is enabled)

## File Storage

Photos and metadata are saved to the SD card with the following structure:
- **Photos**: `/photo.jpg` (latest photo)
- **Metadata**: `/photo_YYYYMMDD_HHMMSS.txt` (timestamped metadata files)

Each metadata file contains:
- Photo title
- Photo description
- Photo URL
- Download timestamp

## Serial Debugging

Connect via USB and monitor the serial port at 115200 baud to see:
- WiFi connection status
- API requests and responses
- JSON parsing results
- SD card operations
- Error messages

## Customization

### Change Photo Source
Replace the `API_URL` constant to use a different API:

```cpp
const char* API_URL = "https://your-api.com/endpoint";
```

### Adjust Display Layout
Modify the `displayPhoto()` function to customize:
- Text positioning
- Font sizes
- Button placement
- Display refresh behavior

### Add Image Display
For JPEG support with grayscale rendering:
1. Add a JPEG decoder library (e.g., `JPEGDEC`)
2. Modify `displayPhoto()` to decode and render the image
3. Convert to grayscale for the E-Ink display

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password in code
- Check WiFi network is 2.4GHz compatible
- Ensure M5Stack is within range

### API Connection Errors
- Check internet connection
- Verify API endpoint is accessible
- Monitor serial output for HTTP error codes
- Check API rate limiting

### SD Card Issues
- Ensure SD card is properly inserted
- Format SD card if not recognized
- Check file system compatibility

### Display Issues
- Refresh display using `M5.Display.flush()`
- Ensure display is properly initialized in `setup()`
- Check font availability and sizes

## API Documentation

For more information about the Boring API, visit:
https://boringapi.com/

API Endpoint: `https://boringapi.com/api/v1/photos/random?num=1`

## License

This project is provided as-is for use with M5Stack devices.

## References

- [M5Stack Documentation](https://docs.m5stack.com/)
- [M5Unified GitHub](https://github.com/m5stack/m5unified)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [ESP32 Arduino Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [Boring API Documentation](https://boringapi.com/)
