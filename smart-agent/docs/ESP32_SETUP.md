# ESP32 Development Guide

This guide covers the development for the Waveshare ESP32-S3 round AMOLED display.

## Hardware Setup

### Required Components
- Waveshare ESP32-S3 1.75inch AMOLED Round Touch Display
- USB-C cable for power and debugging
- Computer with ESP-IDF installed

### Wiring (if needed)
The display module typically has all connections onboard, but verify:
- VCC/GND to 5V and GND
- SPI data lines (SCLK, MOSI, MISO)
- Touch interface (if supported)
- Microphone/Speaker connections

## ESP-IDF Installation

### Linux
```bash
# Install dependencies
sudo apt-get install git flex bison gperf python3 python3-pip cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Clone ESP-IDF
git clone --recursive https://github.com/espressif/ESP-IDF.git
cd ESP-IDF
./install.sh esp32s3

# Source the environment
source export.sh
```

### Windows
Download from [ESP-IDF Download Page](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html#install-esp-idf)

## Waveshare Display Driver

### Installation
```bash
# Clone the driver
git clone https://github.com/waveshare/AMOLED_Display-466x466-EPD
cd AMOLED_Display-466x466-EPD

# Install dependencies
pip3 install RPi.GPIO
```

### Configuration
```bash
idf.py menuconfig
# Set:
# - Component config -> Display
# - Waveshare AMOLED 466x466
# - SPI configuration
```

## Building and Flashing

### Basic Build
```bash
# Build the project
idf.py build
```

### Flash to ESP32
```bash
# Flash the firmware
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

### Monitor Options
```bash
# Stop monitoring
Ctrl+] then q

# Specific serial port
idf.py -p /dev/ttyACM0 monitor
```

## ESP32 Firmware Structure

```
esp32/
├── main/
│   ├── CMakeLists.txt
│   ├── main.c               # Entry point
│   ├── display.c            # Display driver
│   ├── voice.c              # Microphone handling
│   ├── audio.c              # Speaker handling
│   ├── touch.c              # Touch sensor
│   └── wifi.c               # WiFi connectivity
├── components/
│   ├── display_driver/      # AMOLED display driver
│   ├── mic_driver/          # Microphone driver
│   ├── audio_driver/        # Speaker driver
│   └── touch_driver/        # Touch sensor driver
└── sdkconfig                # ESP-IDF configuration
```

## WiFi Connection

### Configure WiFi
Edit `sdkconfig`:
```
CONFIG_ESP_WIFI_SSID="YourSSID"
CONFIG_ESP_WIFI_PASSWORD="YourPassword"
```

### WiFi Management
```c
// Example WiFi connection code
esp_netif_create_default_wifi_sta();
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);
wifi_config_t wifi_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD
    }
};
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
esp_wifi_start();
```

## Display Functions

### Drawing Primitives
```c
// Clear display
display_clear();

// Draw circle
display_draw_circle(x, y, radius, color);

// Draw text
display_draw_text(x, y, "Hello", font, color);

// Update display
display_update();
```

### Round Display Considerations
- Center point: (233, 233) for 466x466
- Text alignment: use circle bounds for positioning
- Animation: smooth transitions for UI elements
- Resolution: 466x466 pixels

## Voice Processing on ESP32

### Microphone Setup
```c
// Initialize I2S for microphone
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};
```

### Audio Streaming
```c
// Read from I2S
size_t bytes_read;
i2s_read(I2S_NUM_0, buffer, buffer_length, &bytes_read, portMAX_DELAY);
```

## Power Management

### Low Power Modes
```c
// Enter deep sleep
esp_deep_sleep_start();

// Enter light sleep
esp_light_sleep_start();
```

### Display Power Control
```c
// Power on/off display
display_power_on();
display_power_off();
```

## Debugging

### Enable Logging
```bash
# Set log level
idf.py menuconfig
# Console -> Log output to UART
# Component config -> ESP32S3-Specific -> Default log verbosity

# Run with verbose logging
idf.py -p /dev/ttyUSB0 -b 115200 -b 460800 monitor
```

### Common Issues
1. **Display not working**: Check SPI connections, verify power supply
2. **WiFi not connecting**: Ensure credentials are correct, check antenna
3. **Audio distortion**: Adjust sample rate, check gain settings
4. **Touch not working**: Verify touch sensor connections, calibrate

## Example Code

See `esp32/` directory for complete example implementations.