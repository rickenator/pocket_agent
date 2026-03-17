# Smart Agent 🤖

An intelligent AI agent for the Waveshare ESP32-S3 1.75" AMOLED round display. This device connects to your WiFi network and acts as your personal AI assistant, providing voice interaction and intelligent responses through Ollama (local) or Gemini (cloud).

## What is Smart Agent?

Smart Agent is a **voice-enabled AI agent** designed for the Waveshare ESP32-S3 with AMOLED display. Unlike a simple chatbot, it's designed to assist you by:

- Listening to your voice commands and questions
- Connecting to local AI models (Ollama) or cloud AI (Gemini)
- Responding with intelligent, helpful answers
- Providing visual feedback on its round AMOLED display
- Making intelligent decisions based on context

The device uses **IoT-style WiFi provisioning** - simply bring your phone near the device, and it creates a local access point named "Buddy-XXXX" to guide you through network setup.

## Features

- **Voice Input**: Speech-to-text with continuous listening
- **AI Agent**: Intelligent responses from local AI models
- **WiFi Provisioning**: Automatic AP-based setup via phone
- **Smart Display**: Beautiful round AMOLED interface
- **Streaming**: Real-time AI responses
- **Local Processing**: Runs on your network, respects privacy
- **Portable**: Compact ESP32-S3 design

## Quick Start

### IoT Provisioning (Recommended for Waveshare Device)

The Smart Agent uses **IoT-style provisioning** for easy WiFi setup:

1. **Power up the device** - The ESP32-S3 creates an AP named "Buddy-XXXX"
2. **Connect your phone** to the "Buddy-XXXX" network
3. **Open a browser** and navigate to `192.168.4.1`
4. **Enter your WiFi credentials** - SSID and password for your main network
5. **Wait for connection** - The device connects to your WiFi and shows Ollama connection info

That's it! Your AI agent is now ready to use.

### Option 1: Local AI (Ollama) - Free & Private

For best performance and privacy, use Ollama running locally on the same network as your ESP32:

```bash
# Install Ollama
brew install ollama  # macOS
curl -fsSL https://ollama.com/install.sh | sh  # Linux
# Download for Windows at https://ollama.com

# Bind to all interfaces so the ESP32 can reach it over WiFi
OLLAMA_HOST=0.0.0.0 ollama serve

# Pull recommended models
ollama pull llama2    # general purpose
ollama pull mistral   # faster, more efficient
```

The ESP32 will connect to Ollama at the server's LAN IP (e.g. `http://192.168.1.251:11434`).

> **Note:** Ollama handles **text processing only**.  Speech-to-text and text-to-speech
> are handled by separate services described below.

### Option 2: Cloud AI (Gemini) - Easy Setup

```bash
# Get API key from https://ai.google.dev/
# Configure settings.yaml with your API key
```

### Option 3: Development Mode

For development on PC without the ESP32 hardware:

```bash
# Run Python version with PC simulation
python main.py
```

## Speech Processing Architecture

Smart Agent separates the three stages of voice interaction — capture, understanding, and
response — across dedicated services.  **Ollama does not process audio**; it receives and
returns text only.

```
ESP32-S3
  │
  │ I2S mic (PCM audio, 16 kHz)
  ▼
STT Service ──────────────────────────────── LAN ──────────────────────
  │  Options:                                                          │
  │    • Whisper HTTP server (e.g. faster-whisper-server)             │
  │      http://192.168.1.251:9000                                    │
  │    • Google Cloud Speech-to-Text REST API                         │
  │  Returns: transcribed text                                        │
  ▼
Ollama LLM ──────────────────────────────── LAN ──────────────────────
  │  http://192.168.1.251:11434                                       │
  │  Models: llama2, mistral, codellama, llama3                       │
  │  Returns: AI response text (streamed)                             │
  ▼
TTS Service ─────────────────────────────── LAN ──────────────────────
  │  Options:                                                         │
  │    • Piper TTS HTTP server                                        │
  │      http://192.168.1.251:5000                                    │
  │    • Google Cloud Text-to-Speech REST API                         │
  │  Returns: WAV audio bytes                                         │
  ▼
AudioDriver (I2S speaker)
```

### Setting Up the Speech Stack

All three services can run on the same LAN machine (e.g. a desktop at `192.168.1.251`):

```bash
# 1. Whisper STT server (faster-whisper-server via Docker)
docker run --gpus all -p 9000:8000 fedirz/faster-whisper-server:latest-cuda
# CPU-only alternative: whisper.cpp --server --port 9000

# 2. Ollama LLM
OLLAMA_HOST=0.0.0.0 ollama serve   # already pulled llama2 / mistral

# 3. Piper TTS server
python -m wyoming_piper --piper /usr/bin/piper \
    --voice en_US-lessac-medium --uri tcp://0.0.0.0:5000
```

Configure the ESP32 firmware to point at these addresses (set in `sdkconfig.defaults` or at
runtime via `AIClient::initAgent()`).

### Python Mode STT/TTS

The Python client (`src/voice/`) uses the same conceptual pipeline but with local libraries:

| Stage | Library | Fallback |
|-------|---------|----------|
| STT | Google Speech Recognition API | OpenAI Whisper (`openai-whisper`) |
| LLM | `OllamaBackend` or `GeminiBackend` | — |
| TTS | macOS `say` / Linux `espeak` / Windows `pyttsx3` | — |

A `MockSpeechRecognizer` is also available in `src/voice/speech.py` for tests that must
run without a microphone or network connection.

## Installation

### For Waveshare ESP32 Device

1. **Clone the repository**
```bash
git clone https://github.com/rickenator/pocket_agent.git
cd smart-agent/esp32
```

2. **Build and flash**
```bash
idf.py build flash monitor
```

3. **Connect to provisioning network**
- Find "Buddy-XXXX" in your WiFi list
- Connect with your phone
- Open `http://192.168.4.1` in browser
- Enter your WiFi credentials
- Device will connect and show Ollama info

### For Development (PC Version)

```bash
# Clone the repository
git clone https://github.com/rickenator/pocket_agent.git
cd smart-agent

# Install Python dependencies
pip install -r requirements.txt

# Configure settings
cp config/settings.example.yaml config/settings.yaml
# Edit with your AI backend configuration
```

## Project Structure

### Waveshare ESP32 Device
```
esp32/
├── main/
│   ├── CMakeLists.txt
│   └── main.cpp              # Main application with WiFi provisioning
├── components/
│   ├── wifi_manager/         # WiFi provisioning (AP mode)
│   ├── web_server/           # Web-based provisioning interface
│   ├── display_driver/       # AMOLED display driver
│   ├── voice_driver/         # I2S microphone
│   ├── audio_driver/         # I2S speaker
│   ├── stt_client/           # HTTP STT client (Whisper / Google)
│   ├── tts_client/           # HTTP TTS client (Piper / Google)
│   └── ai_client/            # Orchestrator: STT → Ollama → TTS
├── CMakeLists.txt
└── README.md
```

### Python Development Version
```
smart-agent/
├── main.py                    # Main application
├── src/
│   ├── ai/                    # AI backends (Ollama, Gemini)
│   ├── voice/                 # Voice processing (mic, speech, TTS)
│   └── ui/                    # Display and widgets
├── config/                    # Configuration files
├── docs/                      # Documentation
└── requirements.txt           # Python dependencies
```

## Documentation

- **[Quick Start Guide](docs/QUICK_START.md)** - Get started in 5 minutes
- **[ESP32 Setup](docs/ESP32_SETUP.md)** - ESP32 development guide
- **[Project Summary](PROJECT_SUMMARY.md)** - Complete overview

## Usage

### Using Your Smart Agent

Once provisioned, your Smart Agent is ready to use:

1. **Speak commands** - Say "What's on my calendar?", "Turn on the lights", or any question
2. **Watch the display** - See the round AMOLED show AI responses and status
3. **Listen to responses** - The device speaks back with AI answers

### Development

### ESP32 Firmware
```bash
cd esp32
idf.py build flash monitor
```

## WiFi Provisioning

The Smart Agent uses **IoT-style provisioning** for simple setup:

### Setup Flow

1. **Device Power On**
   - ESP32-S3 creates an Access Point named "Buddy-XXXX"
   - The network is visible in your phone's WiFi list

2. **Connect Your Phone**
   - Connect to "Buddy-XXXX" (e.g., Buddy-1234)
   - No password required for the provisioning network

3. **Browser Configuration**
   - Open `http://192.168.4.1` in any web browser
   - Enter your WiFi credentials (SSID and password)

4. **Connection Established**
   - Device connects to your WiFi network
   - Displays Ollama server info if available
   - Ready for voice interaction

### Ollama Connection

After provisioning, the device shows:
- Your WiFi IP address (e.g., `192.168.1.100`)
- Ollama server location (if on local network)
- Model name being used

This ensures the device can communicate with your AI backend.

## Configuration

Edit `config/settings.yaml`:

```yaml
ai_backend: "ollama"  # or "gemini"

ollama:
  model: "llama2"
  temperature: 0.7

gemini:
  api_key: "your-api-key"
  model: "gemini-1.5-flash"
```

## Supported AI Models

### Ollama (Local)
- `llama2` - General purpose
- `mistral` - Fast and efficient
- `codellama` - Code generation

### Gemini (Cloud)
- `gemini-1.5-flash` - Fast, free
- `gemini-1.5-pro` - High quality

## Hardware Requirements

### Python Mode
- Python 3.7+
- Microphone (with PyAudio)
- ESP32-S3 (optional for display)

### ESP32 Mode
- ESP32-S3 with WiFi
- Waveshare 1.75" AMOLED Round Display
- Dual microphones
- Touch sensor (optional)

## Development

### Add AI Backend
1. Create file in `src/ai/`
2. Inherit from `AIBackend`
3. Implement `chat()` and `is_available()`

### Add Display Widgets
1. Create in `src/ui/widgets.py`
2. Integrate with `DisplayManager`

## Performance

- **Response Time**: < 2s (local) / < 5s (cloud)
- **Battery Life**: ~2 hours (ESP32)
- **Resolution**: 466x466 pixels
- **Audio Quality**: 16kHz

## Future Enhancements

- 📅 Calendar integration
- 🌤️ Weather services
- 🏠 Smart home control
- 🌐 Web interface
- 📱 Mobile app companion
- 🎤 Voice commands
- 🌍 Multi-language support

## License

MIT License - Feel free to use for personal and educational purposes.

## Support

- 📖 [Documentation](docs/)
- 🐛 [Issues](https://github.com/rickenator/pocket_agent/issues)
- 💬 [Discussions](https://github.com/rickenator/pocket_agent/discussions)

---

Built with ❤️ using Python and ESP32

**Happy Hacking!** 🚀