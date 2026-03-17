# Smart Agent — Architecture

## System Overview

Smart Agent is a voice-enabled AI assistant that runs in two complementary modes:

1. **Python mode** — runs on a PC/Mac/Linux host; uses simulated display; useful for development and testing.
2. **ESP32 firmware mode** — runs natively on the Waveshare ESP32-S3 with AMOLED display; communicates with an Ollama server over WiFi.

```
┌─────────────────────────────────────────────────────┐
│                  Smart Agent System                 │
│                                                     │
│  ┌───────────────┐         ┌─────────────────────┐  │
│  │  Python App   │         │   ESP32-S3 Firmware  │  │
│  │  (main.py)    │         │   (esp32/main/)      │  │
│  └──────┬────────┘         └──────────┬──────────┘  │
│         │                             │              │
│   Voice │ pipeline              WiFi │ HTTP          │
│         │                             │              │
│  ┌──────▼────────┐         ┌──────────▼──────────┐  │
│  │  AI Backend   │         │   Ollama / Gemini    │  │
│  │ (Ollama/Gemini│◄────────│   (remote server)   │  │
│  └───────────────┘         └─────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
smart-agent/
├── main.py                      # Python app entry point
├── test_agent.py                # Automated + interactive test runner
├── requirements.txt             # Python dependencies
├── CMakeLists.txt               # (redundant — see esp32/)
├── config/
│   ├── settings.example.yaml   # Example config (committed)
│   └── settings.yaml           # Local config (gitignored)
├── src/
│   ├── ai/
│   │   ├── base.py             # Abstract AIBackend base class
│   │   ├── ollama.py           # Ollama backend (local LLM)
│   │   └── gemini.py           # Google Gemini backend (cloud)
│   ├── voice/
│   │   ├── mic.py              # MicrophoneManager (PyAudio)
│   │   ├── speech.py           # SpeechRecognizer (Google STT / Whisper)
│   │   └── tts.py              # TextToSpeech (pyttsx3 / system TTS)
│   └── ui/
│       ├── display.py          # DisplayManager (real or simulated)
│       └── widgets.py          # MainWidget, ChatWidget, StatusWidget
├── docs/
│   ├── QUICK_START.md
│   ├── ESP32_SETUP.md
│   └── ESP32_EXAMPLE.c
├── esp32/                       # ESP-IDF firmware project
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp            # FreeRTOS app (WiFi, display, voice, AI)
│   ├── components/
│   │   ├── wifi_manager/       # AP provisioning + STA connection
│   │   ├── web_server/         # Captive portal for WiFi setup
│   │   ├── display_driver/     # AMOLED SPI driver
│   │   ├── voice_driver/       # I2S microphone input
│   │   ├── audio_driver/       # I2S speaker output
│   │   └── ai_client/          # HTTP client for Ollama API
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults      # Board-specific KConfig defaults
│   └── partitions.csv
└── base/home/                   # (purpose TBD — see TODO.md)
```

---

## Component Descriptions

### Python Application (`main.py`)

`SmartAgent` is the top-level orchestrator:

- Loads `config/settings.yaml` with PyYAML.
- Instantiates one AI backend (`OllamaBackend` or `GeminiBackend`).
- Instantiates `DisplayManager` + `MainWidget` for the UI layer.
- Instantiates `MicrophoneManager`, `SpeechRecognizer`, and `TextToSpeech`.
- Runs an async event loop (`asyncio`) driving two possible modes:
  - **`run()`** — waits for a keypress/trigger, listens once, gets a full response, speaks it.
  - **`run_continuous()`** — continuously listens and streams AI response chunks to the display and TTS.

### AI Backends (`src/ai/`)

All backends implement the `AIBackend` abstract base class:

```python
class AIBackend(ABC):
    async def chat(self, message: str, context: list = None) -> AsyncIterator[str]: ...
    async def is_available(self) -> bool: ...
    async def generate(self, message: str, **kwargs) -> str: ...  # convenience wrapper
```

**`OllamaBackend`** — streams from the Ollama `/api/chat` endpoint using `httpx.AsyncClient`. Context is prepended before the new user message so the model sees full history in order.

**`GeminiBackend`** — streams from `generativelanguage.googleapis.com` using the `?key=<api_key>` query parameter (not a Bearer header). Parses NDJSON lines with `json.loads`.

### Voice Pipeline (`src/voice/`)

```
Microphone (PyAudio)
      │
      ▼
MicrophoneManager     — captures raw PCM audio frames
      │
      ▼
SpeechRecognizer      — converts audio to text (Google Speech API / Whisper)
      │
      ▼
  text string          → passed to AI backend
      │
      ▼
TextToSpeech          — converts AI response text to audio playback
```

### Display / UI (`src/ui/`)

- `DisplayManager` — abstracts the AMOLED display; in simulation mode (no hardware) it renders to a Pillow canvas or ignores calls.
- `MainWidget` — composite widget containing:
  - `ChatWidget` — scrolling conversation history
  - `StatusWidget` — AI backend name and availability indicator
  - `InputWidget` — microphone activity visualisation

### ESP32 Firmware (`esp32/`)

A pure C/C++ ESP-IDF project targeting the Waveshare ESP32-S3:

| Component | Responsibility |
|-----------|----------------|
| `wifi_manager` | Starts an AP ("Buddy-XXXX"), handles STA join, stores credentials in NVS |
| `web_server` | Captive portal at `192.168.4.1` for credential entry |
| `display_driver` | SPI driver for the 466×466 AMOLED panel |
| `voice_driver` | I2S microphone capture |
| `audio_driver` | I2S speaker playback |
| `ai_client` | HTTP/1.1 streaming client to Ollama on the local network |

The firmware communicates with **Ollama** (or any compatible HTTP endpoint) over the local WiFi network — it does **not** bundle the Python app.

---

## Data Flow

### Python Mode

```
User speaks
    │
    ▼
MicrophoneManager.record()
    │
    ▼
SpeechRecognizer.listen() → text string
    │
    ▼
SmartAgent: display "Thinking..." + pass text to backend
    │
    ▼
OllamaBackend.chat(text) / GeminiBackend.chat(text)
    │   streams chunks
    ▼
SmartAgent: accumulate chunks → full response string
    │
    ├─► DisplayManager.show_text(chunk)   (streamed, per chunk)
    │
    └─► TextToSpeech.speak(response)      (after full response assembled)
```

### ESP32 Mode

```
I2S microphone → PCM audio
    │
    ▼
(STT on-device or forwarded to host — TBD)
    │
    ▼
ai_client: HTTP POST to Ollama /api/chat
    │   streams NDJSON response
    ▼
Parse → extract content chunks
    │
    ├─► AMOLED display update
    │
    └─► I2S speaker / TTS output
```

---

## Configuration System

Settings are loaded from `config/settings.yaml` (local, gitignored) with fallback to `config/settings.example.yaml`.

```yaml
ai_backend: "ollama"          # "ollama" | "gemini"

ollama:
  host: "http://localhost:11434"
  model: "llama2"             # any model pulled in Ollama
  temperature: 0.7
  max_tokens: 500

gemini:
  api_key: "YOUR_GEMINI_API_KEY"
  model: "gemini-1.5-flash"
  temperature: 0.7
  max_tokens: 500

display:
  width: 466
  height: 466
  rotation: 0
  brightness: 255

voice:
  sample_rate: 16000
  chunk_size: 1024
  energy_threshold: 1000
  silence_timeout: 3
```

---

## Network Architecture (ESP32)

```
Phone / laptop
      │
      │ (1) Connects to "Buddy-XXXX" AP
      ▼
ESP32-S3 (AP mode, 192.168.4.1)
      │
      │ (2) User enters home WiFi credentials via browser
      ▼
ESP32-S3 (STA mode, joins home network)
      │
      │ (3) HTTP requests to Ollama
      ▼
Ollama server (same local network)
```

---

## ESP32 vs Python Mode Differences

| Feature | Python Mode | ESP32 Mode |
|---------|-------------|------------|
| AI backend | Ollama or Gemini | Ollama (HTTP) |
| STT | Google Speech API / Whisper | On-device / future |
| TTS | pyttsx3 / system | I2S speaker |
| Display | Simulated (Pillow) or real | Waveshare AMOLED SPI |
| Deployment | PC / Mac / Linux | Waveshare ESP32-S3 board |
| WiFi setup | N/A (uses host network) | AP provisioning portal |

---

## Adding a New AI Backend

1. Create `src/ai/my_backend.py`.
2. Subclass `AIBackend` from `src/ai/base.py`.
3. Implement `async def chat(...)` (yields string chunks) and `async def is_available(...)`.
4. Register in `SmartAgent.setup_ai_backend()` in `main.py`.
5. Add any required packages to `requirements.txt`.
