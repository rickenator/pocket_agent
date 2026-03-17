# Smart Agent Project Summary

## Overview

A voice-enabled AI assistant built around the Waveshare ESP32-S3 1.75" AMOLED round display. The system uses Python for AI processing and can optionally run firmware on ESP32 for voice processing.

## Project Structure

```
smart-agent/
├── main.py                    # Main application entry point
├── requirements.txt           # Python dependencies
├── .gitignore                 # Git ignore patterns
├── config/
│   └── settings.example.yaml  # Example configuration file
├── src/
│   ├── ai/                    # AI Backend modules
│   │   ├── __init__.py
│   │   ├── base.py           # Abstract base class
│   │   ├── ollama.py         # Ollama backend implementation
│   │   └── gemini.py         # Google Gemini backend implementation
│   ├── voice/                 # Voice processing modules
│   │   ├── __init__.py
│   │   ├── mic.py            # Microphone manager
│   │   ├── speech.py         # Speech recognition
│   │   └── tts.py            # Text-to-speech
│   └── ui/                   # UI Display modules
│       ├── __init__.py
│       ├── display.py        # Display manager
│       └── widgets.py        # UI widgets
└── docs/
    ├── QUICK_START.md        # Quick start guide
    ├── ESP32_SETUP.md        # ESP32 development guide
    └── ESP32_EXAMPLE.c       # ESP32 C example code
```

## Key Components

### 1. AI Backends

**Ollama Backend** (`src/ai/ollama.py`)
- Local LLM inference using Ollama
- Free and private
- Support for multiple models (Llama2, Mistral, etc.)
- Streaming response support

**Gemini Backend** (`src/ai/gemini.py`)
- Google Gemini AI
- Cloud-based with free tier
- Streaming response support
- Easy to configure with API key

### 2. Voice Processing

**Microphone Manager** (`src/voice/mic.py`)
- Dual microphone array support
- Real-time audio streaming
- Energy-based voice activity detection
- Compatible with PyAudio

**Speech Recognition** (`src/voice/speech.py`)
- Speech-to-text conversion
- Google Speech Recognition API (cloud)
- Optional OpenAI Whisper support (local)
- `MockSpeechRecognizer` class for unit tests (no microphone required)
- Configurable language and energy parameters

**Text-to-Speech** (`src/voice/tts.py`)
- System text-to-speech synthesis
- Multi-platform: macOS `say`, Linux `espeak`, Windows `pyttsx3`
- Blocking (`speak()`) and non-blocking (`speak_async()`) modes
- Streaming chunk callbacks (`speak_stream()`)

### 3. UI Display

**Display Manager** (`src/ui/display.py`)
- Round AMOLED display support
- PC simulation mode (no hardware)
- Basic drawing primitives
- Text rendering with fonts

**UI Widgets** (`src/ui/widgets.py`)
- Chat widget for conversation display
- Status widget for system info
- Input widget for voice indicators
- Main widget combining all elements

## Supported Platforms

### Python Mode
- **Windows**: Full support
- **macOS**: Full support
- **Linux**: Full support

### ESP32 Mode
- **ESP32-S3**: Native firmware
- **Waveshare AMOLED Display**: 1.75" round
- **Dual microphones**: Array support
- **Touch sensor**: Optional integration

## AI Models

### Ollama Models (Local)
- `llama2` - General purpose
- `mistral` - Fast and efficient
- `codellama` - Code generation
- `llama3` - Latest generation

### Gemini Models (Cloud)
- `gemini-1.5-flash` - Fast, free
- `gemini-1.5-pro` - High quality

## Features

✅ **Voice Input**: Speech-to-text with continuous listening
✅ **AI Processing**: Local (Ollama) or Cloud (Gemini)
✅ **Voice Output**: Text-to-speech with multiple backends
✅ **Display**: Round AMOLED with visual feedback
✅ **Streaming**: Real-time response streaming
✅ **WiFi**: Network connectivity for ESP32
✅ **Power Management**: Low power consumption
✅ **Modular Design**: Easy to extend and customize

## Installation

### Python Dependencies
```bash
pip install -r requirements.txt
```

### System Dependencies
- Python 3.7+
- PyAudio (for microphone)
- Pillow (for display)
- SpeechRecognition (for STT)
- Google API client (for Gemini)
- Optional: OpenAI (for Whisper)

### ESP-IDF
See `docs/ESP32_SETUP.md`

## Configuration

Edit `config/settings.yaml`:

```yaml
# AI Backend
ai_backend: "ollama"  # or "gemini"

# Ollama Settings
ollama:
  host: "http://localhost:11434"
  model: "llama2"
  temperature: 0.7
  max_tokens: 500

# Gemini Settings
gemini:
  api_key: "your-api-key"
  model: "gemini-1.5-flash"
  temperature: 0.7
  max_tokens: 500

# Display Settings
display:
  width: 466
  height: 466
  rotation: 0
  brightness: 255

# Voice Settings
voice:
  sample_rate: 16000
  chunk_size: 1024
  energy_threshold: 1000
  silence_timeout: 3
```

## Usage

### Basic Usage
```bash
python main.py
```

### Continuous Listening
```bash
python main.py --continuous
```

### ESP32 Firmware
```bash
cd esp32
idf.py build flash monitor
```

## Development

### Adding New AI Backend
1. Create new file in `src/ai/`
2. Inherit from `AIBackend`
3. Implement `chat()` and `is_available()`
4. Update `src/ai/__init__.py`

### Adding New UI Components
1. Create new file in `src/ui/`
2. Add new widgets in `src/ui/widgets.py`
3. Integrate with main display manager

### Adding Voice Features
1. Extend microphone manager in `src/voice/mic.py`
2. Add new recognition methods in `src/voice/speech.py`
3. Implement TTS backends in `src/voice/tts.py`

## Performance

- **Latency**: < 2s (local) / < 5s (cloud)
- **Battery**: ~2 hours (ESP32)
- **Memory**: ESP32-S3 8MB
- **Display**: 466x466 pixels
- **Sample Rate**: 16kHz audio

## ESP32 Speech-to-Text / Text-to-Speech Integration

The ESP32 firmware delegates all speech processing to services on the local network.
Ollama handles **text generation only** — it does not process audio.

### Pipeline

```
I2S Microphone (PCM audio, 16 kHz)
         │
         ▼
   STTClient (stt_client component)
         │  HTTP POST WAV → Whisper or Google STT server
         │  Returns: transcript text
         ▼
   OllamaClient (ai_client component)
         │  HTTP POST /api/chat → Ollama LLM server
         │  Returns: AI response text (streamed)
         ▼
   TTSClient (tts_client component)
         │  HTTP POST text → Piper TTS or Google TTS server
         │  Returns: WAV audio bytes
         ▼
   AudioDriver → I2S Speaker
```

### Recommended Server Setup (all on one LAN machine, e.g. 192.168.1.251)

| Service | Default Port | Software |
|---------|-------------|---------|
| Whisper STT | 9000 | faster-whisper-server or whisper.cpp |
| Ollama LLM | 11434 | Ollama (`OLLAMA_HOST=0.0.0.0 ollama serve`) |
| Piper TTS | 5000 | wyoming-piper |

### Supported STT Providers

| Provider | Type | Notes |
|----------|------|-------|
| Whisper (local) | faster-whisper-server or whisper.cpp | Private; runs entirely on LAN |
| Google Cloud Speech | Cloud REST API | Requires API key |

### Supported TTS Providers

| Provider | Type | Notes |
|----------|------|-------|
| Piper TTS (local) | wyoming-piper HTTP server | Private; high quality; many voices |
| eSpeak-ng (local) | eSpeak HTTP wrapper | Lightweight fallback |
| Google Cloud TTS | Cloud REST API | Requires API key |
| ElevenLabs | Cloud REST API | Requires API key; highest quality |

### ESP32 Component Summary

| Component | Responsibility |
|-----------|----------------|
| `voice_driver` | I2S microphone capture |
| `stt_client` | HTTP client → STT service; returns transcript |
| `ai_client` | Orchestrates full pipeline; Ollama LLM tool-call loop |
| `tts_client` | HTTP client → TTS service; plays WAV via `audio_driver` |
| `audio_driver` | I2S speaker playback |



- [ ] Multiple AI model support
- [ ] Context-aware conversations
- [ ] Calendar integration
- [ ] Weather services
- [ ] Home automation control
- [ ] Smart home integration
- [ ] Web interface
- [ ] Mobile app companion
- [ ] Enhanced voice commands
- [ ] Multiple language support
- [ ] Advanced TTS customization

## License

This project is open source and available for educational and personal use.

## Contributing

Contributions are welcome! Areas of focus:
- AI backend improvements
- Display driver enhancements
- Voice processing optimizations
- ESP32 firmware development
- UI/UX improvements

## Support

For issues and questions:
1. Check `docs/QUICK_START.md` for setup
2. See `docs/ESP32_SETUP.md` for ESP32 development
3. Review the example code in `docs/ESP32_EXAMPLE.c`

## Acknowledgments

- Waveshare for the display hardware
- Ollama for the local LLM framework
- Google for Gemini AI
- ESP-IDF team for the ESP32 development tools
- Open source community

---

**Built with ❤️ for makers and AI enthusiasts**