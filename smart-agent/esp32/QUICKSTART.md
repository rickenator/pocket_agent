# Quick Start Guide

## Prerequisites

### 1. Install ESP-IDF v5.x

```bash
# Clone and install ESP-IDF
git clone --recursive https://github.com/espressif/ESP-IDF.git
cd ESP-IDF
./install.sh esp32s3

# Source the environment
source export.sh
```

### 2. Verify Installation

```bash
# Check ESP-IDF version
esp_idf_version
```

## Quick Build & Flash

### Windows (Command Prompt)

```cmd
cd smart-agent\esp32

# Source ESP-IDF environment
call esp-idf\export.bat

# Build
idf.py build

# Flash and monitor
idf.py -p COM3 flash monitor
```

### macOS/Linux

```bash
cd smart-agent/esp32

# Source ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

### 1. WiFi Configuration

Edit `sdkconfig` and set:

```ini
CONFIG_ESP_WIFI_SSID="YourSSID"
CONFIG_ESP_WIFI_PASSWORD="YourPassword"
```

Or use menuconfig:

```bash
idf.py menuconfig
# Navigate to:
#   Component config -> Wi-Fi
#   -> Station
#   -> Set SSID and Password
```

### 2. Display Mode

Edit `main/main.cpp`:

```cpp
// For development with PC simulation:
g_display->init(DISPLAY_SIMULATION);

// For hardware:
g_display->init(DISPLAY_ESP32_HARDWARE);
```

### 3. AI Backend

Edit `main/main.cpp`:

```cpp
// For Ollama (local):
g_ai->init(AI_BACKEND_OLLAMA);

// For Gemini (cloud):
g_ai->init(AI_BACKEND_GEMINI);
```

## Testing

### Run the Firmware

1. Flash the firmware to your ESP32-S3:
   ```bash
   ./flash.sh /dev/ttyUSB0
   ```

2. Monitor the output:
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

### Expected Output

```
I (0) Main: === Smart Agent ESP32-S3 ===
I (1) NVS: Initializing NVS
I (2) Main: All components initialized
I (3) Main: Application started
I (4) Main: Press Ctrl+C to stop
I (5) Voice task started
I (6) Display task started
I (7) Voice task: Voice detected!
I (8) Display task: Status: 🎤 Listening...
```

## Troubleshooting

### Build Errors

**Error: "idf.py not found"**
- Make sure ESP-IDF is properly installed
- Source the export script before building

**Error: " кого cmake not found"**
- Install CMake (esp-idf requires v3.5+)

**Error: " gcc not found"**
- Install GCC compiler
- On Ubuntu: `sudo apt-get install gcc g++`

### Flashing Errors

**Error: "Failed to open serial port"**
- Check USB connection
- Try different USB port or cable
- On Linux, add user to `dialout` group: `sudo usermod -a -G dialout $USER`

**Error: "Chip is not found"**
- Check GPIO connections
- Verify ESP32-S3 is powered
- Try resetting the board

### Runtime Errors

**Display not showing**
- Ensure display is initialized in hardware mode
- Check SPI connections
- Verify power supply

**Voice not detected**
- Check microphone connections
- Verify I2S GPIO pins
- Adjust silence threshold in `voice.h`

**AI not responding**
- Ensure Ollama is running: `ollama serve`
- Check localhost:11434 is accessible
- Verify model is available

## Next Steps

1. ✅ Build and flash the firmware
2. ✅ Test voice detection
3. ✅ Verify display updates
4. 🔄 Add display driver for Waveshare AMOLED
5. 🔄 Implement touch sensor
6. 🔄 Add WiFi connection management
7. 🔄 Optimize for production

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1-n8r2-datasheet-en.pdf)
- [Waveshare AMOLED Display](https://www.waveshare.com/1.75inch-amoled-round-lcd.htm)
- [Ollama Documentation](https://ollama.ai/docs)

## Support

- ESP-IDF Forum: https://forum.espressif.com/
- ESP32-S3 Discord: https://discord.gg/esp32

## Contributing

Feel free to submit issues and pull requests!