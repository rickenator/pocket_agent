# ESP32-S3 Smart Agent Firmware

An intelligent AI agent for the Waveshare ESP32-S3 1.75" AMOLED round display with IoT-style WiFi provisioning. The Smart Agent is designed to be your personal assistant, providing voice interaction and intelligent responses through Ollama (local) or Gemini (cloud).

## Project Structure

```
esp32/
├── main/
│   ├── CMakeLists.txt
│   └── main.cpp          # Main application with WiFi provisioning
├── components/
│   ├── wifi_manager/     # WiFi AP and station mode
│   ├── web_server/       # Provisioning web interface
│   ├── display_driver/    # AMOLED round display driver
│   ├── voice_driver/      # I2S microphone driver
│   ├── audio_driver/      # I2S speaker driver
│   ├── ai_client/         # AI communication client
│   └── include/
│       └── display_driver.h
├── CMakeLists.txt
└── README.md
```

## Hardware Requirements

- ESP32-S3 devkit board
- Waveshare 1.75" AMOLED round display (466x466)
- Microphone (I2S)
- Speaker (I2S)
- USB-C cable

## Software Requirements

- ESP-IDF v5.x
- CMake
- Python 3.x
- GCC compiler

## Building

### 1. Install ESP-IDF

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/ESP-IDF.git
cd ESP-IDF
./install.sh esp32s3

# Source the environment
source export.sh
```

### 2. Configure the Project

```bash
cd smart-agent/esp32

# Copy ESP-IDF template
idf.py fullclean

# Configure project
idf.py menuconfig
```

### 3. Build

```bash
# Build the firmware
idf.py build
```

## Flashing

### Using esptool

```bash
# Flash to ESP32-S3
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

### Using VS Code

1. Open the project in VS Code
2. Press F5 to build and flash
3. Use the integrated terminal for monitoring

## Configuration

### Display Mode

Edit `main/main.cpp`:

```cpp
g_display->init(DISPLAY_SIMULATION);  // PC simulation mode
// or
g_display->init(DISPLAY_ESP32_HARDWARE);  // Hardware mode
```

### AI Backend

Edit `main/main.cpp`:

```cpp
g_ai->init(AIουνLLAMA);  // Ollama backend
// or
g_ai->init(AI_BACKEND_GEMINI);  // Gemini backend
```

### WiFi Configuration

The Smart Agent uses **IoT-style provisioning** - no manual config needed!

1. Power on the device
2. Connect phone to "Buddy-XXXX" network
3. Open http://192.168.4.1 in browser
4. Enter WiFi credentials
5. Device auto-connects to main WiFi

### Ollama Configuration

After provisioning, configure Ollama in `main/main.cpp`:

```cpp
g_ai->init(AI_BACKEND_OLLAMA);  // Ollama backend
// Set model in components/ai_client/
```

## Features

### WiFi Provisioning (IoT-Style)
- **AP Mode**: Creates "Buddy-XXXX" network for easy setup
- **Web Interface**: Browser-based credential entry
- **Smart Setup**: Automatic connection to main WiFi
- **Status Display**: Shows IP address and Ollama connection info

### Display Driver
- Round AMOLED display support (466x466)
- PC simulation mode for development
- Drawing primitives: circle, text, rectangle
- UI helpers: chat bubble, status indicator, voice indicator

### Voice Driver
- I2S microphone input (16kHz sample rate)
- Voice detection algorithm
- Gain control
- Silence threshold configuration

### Audio Driver
- I2S speaker output
- Volume control
- Playback state management

### AI Client
- Ollama backend support
- Streaming chat completion
- Configuration: model, temperature, max tokens
- HTTP client for API communication

### Agent Capabilities
- Voice command processing
- Context-aware responses
- Status monitoring
- Smart decision making

## FreeRTOS Tasks

### WiFi Task
- Manages AP mode for provisioning
- Handles station mode for main network
- Monitors connection status
- Triggers Ollama info display

### Voice Task
- Monitors microphone input
- Detects voice activity
- Handles voice processing
- Manages AI agent interactions

### Display Task
- Updates display UI
- Shows AI responses in real-time
- Handles events from other tasks
- Manages display state and animations

### AI Task
- Communicates with Ollama/Gemini backend
- Processes streaming responses
- Manages conversation context
- Implements agent logic

### Agent Task
- Makes intelligent decisions
- Maintains conversation state
- Manages device operations
- Provides status updates

## Development Mode

During development, the firmware uses **PC simulation mode** for the display, allowing you to test the logic without hardware. Once the display hardware is connected, simply change the mode in `main/main.cpp`:

```cpp
g_display->init(DISPLAY_ESP32_HARDWARE);
```

## Smart Agent Capabilities

The Smart Agent is designed as an intelligent assistant, not just a chatbot. Its key agent capabilities include:

### Voice Understanding
- Natural speech recognition
- Context-aware command parsing
- Voice activity detection

### AI Integration
- **Local AI**: Ollama with llama2, mistral, or codellama
- **Cloud AI**: Gemini API for advanced capabilities
- Streaming responses for real-time interaction

### Intelligent Decision Making
- Context maintenance across conversations
- Status monitoring and awareness
- Adaptive responses based on user behavior

### Hardware Interaction
- Display management with round AMOLED
- Voice input and output
- Status indicators for system state

### IoT-Ready
- WiFi provisioning for easy setup
- Network-aware operation
- Automatic discovery of AI backend

## Troubleshooting

### Display not working
- Check SPI connections
- Verify power supply (3.3V or 5V as required)
- Ensure display is initialized in hardware mode

### Voice not detected
- Check microphone connections
- Verify I2S configuration (GPIO pins)
- Adjust silence threshold in `voice.h`

### AI not responding
- Ensure Ollama is running on localhost:11434
- Check WiFi connectivity
- Verify model is available in Ollama

## Next Steps

1. Test the firmware on ESP32-S3 with PC simulation
2. Add display driver for Waveshare AMOLED
3. Implement touch sensor driver
4. Add WiFi connection management
5. Implement complete UI with animations
6. Add error handling and logging
7. Optimize for power consumption

## License

MIT License

## Author

Created for ESP32-S3 Smart Agent project.