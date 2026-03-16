# ESP32-S3 Smart Agent Firmware - Project Summary

## Overview

 độngESP32-S3 firmware for a voice-interactive smart agent with a Waveshare AMOLED round display. The project implements a modular architecture with separate drivers for display, voice, audio, and AI communication.

## Project Structure

```
esp32/
├── main/
│   ├── CMakeLists.txt          # Main application build configuration
│   └── main.cpp                # Main application with FreeRTOS tasks
├── components/
│   ├── display_driver/         # Display driver with PC simulation
│   │   ├── CMakeLists.txt
│   │   ├── display.cpp         # Display implementation
│   │   └── display.h           # Display interface
│   ├── voice_driver/           # I2S microphone driver
│   │   ├── CMakeLists.txt
│   │   ├── voice.cpp           # Voice implementation
│   │   └── voice.h             # Voice interface
│   ├── audio_driver/           # I2S speaker driver
│   │   ├── CMakeLists.txt
│   │   ├── audio.cpp           # Audio implementation
│   │   └── audio.h             # Audio interface
│   ├── ai_client/              # AI communication client
│   │   ├── CMakeLists.txt
│   │   ├── ai_client.cpp       # AI implementation
│   │   └── ai_client.h         # AI interface
│   ├── include/
│   │   └── display_driver.h    # Public header for display driver
│   └── CMakeLists.txt          # Component build configuration
├── test_display_sim.cpp        # Python display simulation script
├── build.sh                    # Build script
├── flash.sh                    # Flash and monitor script
├── CMakeLists.txt              # Root CMake configuration
├── README.md                   # Project documentation
├── QUICKSTART.md               # Quick start guide
└── PROJECT_SUMMARY.md          # This file
```

## Core Components

### 1. Display Driver (`display_driver/`)

**Purpose:** Manages the Waveshare AMOLED round display (466x466)

**Features:**
- PC simulation mode for development
- Hardware mode for ESP32-S3
- Drawing primitives: circle, text, rectangle
- UI helpers: chat bubble, status indicator, voice indicator

**Key Classes:**
- `DisplayDriver`: Main display interface

**Configuration:**
- Display size: 466x466 pixels
- Center position: (233, 233)
- 8 predefined colors

### 2. Voice Driver (`voice_driver/`)

**Purpose:** Handles I2S microphone input and voice detection

**Features:**
- I2S microphone input (16kHz sample rate)
- Voice detection algorithm (RMS-based)
- Gain control (0-5x)
- Silence threshold configuration
- Dual I2S instances (one for mic, one for speaker)

**Key Classes:**
- `VoiceDriver`: Main voice interface

**Configuration:**
- Sample rate: 16kHz
- Chunk size: 1024 samples
- Silence threshold: 1000
- Silence timeout: 300ms

**GPIO Configuration:**
- I2S BCK: GPIO 26
- I2S WS: GPIO 25
- I2S SD (mic): GPIO 17
- I2S SD (spk): GPIO 33

### 3. Audio Driver (`audio_driver/`)

**Purpose:** Manages I2S speaker output for voice playback

**Features:**
- I2S speaker output
- Volume control (0-100%)
- Playback state management
- Auto-clear TX descriptor

**Key Classes:**
- `AudioDriver`: Main audio interface

### 4. AI Client (`ai_client/`)

**Purpose:** Handles AI communication for natural language processing

**Features:**
- Multiple AI backend support (Ollama, Gemini)
- Streaming chat completion
- HTTP client for API communication
- Configurable: model, temperature, max tokens

**Key Classes:**
- `AI`: Main AI interface

**Backend Support:**
- **Ollama**: Local LLM (default)
  - Endpoint: `http://localhost:11434/api/chat`
  - Streaming: Enabled
- **Gemini**: Google AI (future)
  - Cloud-based API
  - Streaming support

## FreeRTOS Architecture

### Tasks

**Voice Task (Core 1)**
- Priority: 5
- Stack: 4096 words
- Function: Monitors microphone, detects voice, triggers processing

**Display Task (Core 0)**
- Priority: 5
- Stack: 4096 words
- Function: Updates display UI, handles events

### Event Queue

- Size: 10 events
- Event types:
  - `EVENT_VOICE_DETECTED`: Voice activity detected
  - `EVENT_VOICE_COMPLETE`: Voice processing complete
  - `EVENT_DISPLAY_UPDATE`: Display update requested
  - `EVENT_AI_RESPONSE`: AI response received
  - `EVENT_WIFI_CONNECTED`: WiFi connection established

## Build System

### Build Commands

```bash
# Configure
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Or use scripts
./build.sh
./flash.sh /dev/ttyUSB0
```

### Dependencies

- ESP-IDF v5.x
- CMake
- Python 3.x
- GCC compiler
- ESP32-S3 chip

## Development Features

### PC Simulation Mode

For testing without hardware:

1. Set display mode to simulation:
   ```cpp
   g_display->init(DISPLAY_SIMULATION);
   ```

2. Use `test_display_sim.cpp` to simulate display operations

3. Monitor logs in terminal for visual feedback

### Hardware Mode

For production:

1. Set display mode to hardware:
   ```cpp
   g_display->init(DISPLAY_ESP32_HARDWARE);
   ```

2. Connect Waveshare AMOLED display

3. Verify SPI connections and power supply

## Display Modes

**DISPLAY_SIMULATION** (default)
- PC-based text output
- No hardware dependencies
- Ideal for development and testing

**DISPLAY_ESP32_HARDWARE**
- Actual ESP32-S3 display output
- Requires Waveshare AMOLED connection
- Full hardware functionality

## AI Backend Configuration

**Ollama (Recommended)**
1. Start Ollama server:
   ```bash
   ollama serve
   ```

2. Pull desired model:
   ```bash
   ollama pull llama2
   ```

3. Configure in code:
   ```cpp
   g_ai->init(AI_BACKEND_OLLAMA);
   g_ai->setModel("llama2");
   ```

**Gemini (Future)**
1. Set API key in `sdkconfig`
2. Configure in code:
   ```cpp
   g_ai->init(AI_BACKEND_GEMINI);
   ```

## Key Design Decisions

1. **Modular Architecture**: Separate drivers for each hardware component
2. **PC Simulation**: Display can be tested without hardware
3. **FreeRTOS**: Parallel task execution for responsiveness
4. **Event-Driven**: Asynchronous communication between components
5. **Extensible**: Easy to add new features (WiFi, touch, etc.)

## Next Steps

### Phase 1: Testing (Current)
- ✅ Build firmware
- ✅ Test display simulation
- ✅ Verify voice detection
- 🔄 Test on ESP32-S3 hardware
- 🔄 Debug and optimize

### Phase 2: Hardware Integration
- Add Waveshare AMOLED driver
- Connect microphone and speaker
- Implement touch sensor driver
- Add WiFi connection management

### Phase 3: Features
- Complete UI with animations
- Error handling and recovery
- Power optimization
- OTA updates
- Custom voice commands

### Phase 4: Production
- Complete testing
- Documentation
- Power consumption optimization
- Manufacturing guidelines

## Known Limitations

1. Display driver for Waveshare AMOLED needs implementation
2. Touch sensor not yet implemented
3. WiFi connection not yet implemented
4. Error handling could be more robust
5. Power consumption not optimized

## Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1-n8r2-datasheet-en.pdf)
- [Waveshare AMOLED Display](https://www.waveshare.com/1.75inch-amoled-round-lcd.htm)
- [Ollama Documentation](https://ollama.ai/docs)
- [FreeRTOS Documentation](https://freertos.org/)

## License

MIT License - See LICENSE file for details

## Author

Created as part of the Smart Agent project.

## Version

v1.0.0 - Initial firmware structure