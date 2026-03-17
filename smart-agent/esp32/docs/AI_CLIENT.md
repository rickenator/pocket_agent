# AI Client Documentation

## Overview

The Smart Agent AI Client provides the complete speech-to-response pipeline for the ESP32
firmware.  Raw PCM audio captured by the I2S microphone is transcribed by an external
Speech-to-Text (STT) service, the resulting text is processed by a local Ollama LLM server,
and the LLM's reply is converted back to speech by an external Text-to-Speech (TTS) service
before being played through the I2S speaker.

## End-to-End Speech Pipeline

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ESP32-S3 Firmware                             │
│                                                                      │
│  ┌──────────────┐    PCM audio     ┌──────────────────────────────┐  │
│  │ Voice Driver │ ───────────────► │         AI Client            │  │
│  │ (I2S mic)    │                  │                              │  │
│  └──────────────┘                  │  ┌────────────┐              │  │
│                                    │  │ STT Client │              │  │
│                                    │  └─────┬──────┘              │  │
│                                    │        │ transcript           │  │
│                                    │  ┌─────▼──────┐              │  │
│                                    │  │Ollama Client│             │  │
│                                    │  └─────┬──────┘              │  │
│                                    │        │ LLM response         │  │
│                                    │  ┌─────▼──────┐              │  │
│                                    │  │ TTS Client │              │  │
│                                    │  └─────┬──────┘              │  │
│  ┌──────────────┐    WAV audio     │        │                      │  │
│  │ Audio Driver │ ◄────────────────┘  ┌─────▼──────┐              │  │
│  │ (I2S speaker)│                  │  │ AudioDriver│              │  │
│  └──────────────┘                  │  └────────────┘              │  │
│                                    └──────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
         │                                       │
         │ WiFi                                  │ WiFi
         ▼                                       ▼
┌─────────────────────┐              ┌──────────────────────┐
│ STT Server          │              │ Ollama Server        │
│ (e.g. Whisper)      │              │ (LLM inference)      │
│ 192.168.1.251:9000  │              │ 192.168.1.251:11434  │
└─────────────────────┘              └──────────────────────┘
         ▲
         │ WiFi
┌─────────────────────┐
│ TTS Server          │
│ (e.g. Piper TTS)    │
│ 192.168.1.251:5000  │
└─────────────────────┘
```

## Components

### AIClient

The top-level orchestrator that wires together STT, Ollama LLM, and TTS into a single
call (`processVoiceAudio`).

**Features:**
- Full voice pipeline: PCM → STT → Ollama → TTS → audio playback
- Text query processing (skips STT/TTS steps)
- Tool-call agent loop with GoalManager and ToolRegistry
- Conversation history via MemoryManager

### STTClient (`esp32/components/stt_client/`)

Sends raw PCM audio to an external Speech-to-Text service and returns the transcript.

**Supported providers:**
| Provider | Transport | Notes |
|----------|-----------|-------|
| Whisper (local) | HTTP POST multipart/form-data | faster-whisper-server or whisper.cpp |
| Google Cloud STT | HTTPS REST | Requires API key |

**Features:**
- Automatic WAV header generation from raw PCM
- Configurable retry logic (default 3 attempts)
- Configurable timeout (default 10 s)
- Language selection (BCP-47 tags, e.g. `en-US`)

### OllamaClient (`esp32/components/ai_client/`)

HTTP client for chat completions against a local Ollama server.

**Features:**
- Chat completion API (`/api/chat`)
- Streaming responses (SSE / NDJSON)
- Model listing (`/api/tags`)
- Health checks
- Conversation history with role tracking
- Configurable retry logic

**Status:** Core implementation (HTTP client pending)

### TTSClient (`esp32/components/tts_client/`)

Converts text to speech by calling an external TTS service and plays the returned audio
through the I2S speaker via AudioDriver.

**Supported providers:**
| Provider | Transport | Notes |
|----------|-----------|-------|
| Piper TTS (local) | HTTP POST JSON | Runs on the same LAN server |
| eSpeak-ng (local) | HTTP POST | Lightweight fallback |
| Google Cloud TTS | HTTPS REST | Requires API key |
| ElevenLabs | HTTPS REST | Requires API key; high quality |

**Features:**
- Blocking and non-blocking playback
- Streaming chunk callback for progress indicators
- Configurable retry logic (default 3 attempts)
- Configurable timeout (default 15 s)

## Configuration

### Server Configuration

All three servers are expected to run on the same local network as the ESP32.  A common
setup uses a single machine (e.g. a desktop or NAS) at `192.168.1.251`:

| Service | Port | URL |
|---------|------|-----|
| Whisper STT | 9000 | `http://192.168.1.251:9000` |
| Ollama LLM | 11434 | `http://192.168.1.251:11434` |
| Piper TTS | 5000 | `http://192.168.1.251:5000` |

Configure the ESP32 firmware accordingly:

```cpp
// Ollama LLM server
ollamaClient->setServerUrl("http://192.168.1.251:11434");
ollamaClient->setModel("llama2");  // or "mistral", "llama3", etc.

// STT server (Whisper)
sttClient->setProvider(STT_PROVIDER_WHISPER);
sttClient->setServerUrl("http://192.168.1.251:9000");
sttClient->setLanguage("en-US");
sttClient->setMaxRetries(3);

// TTS server (Piper)
ttsClient->setProvider(TTS_PROVIDER_PIPER);
ttsClient->setServerUrl("http://192.168.1.251:5000");
ttsClient->setVoice("en_US-lessac-medium");
ttsClient->setSampleRate(22050);
```

### Full Agent Initialisation

```cpp
AIClient aiClient;
AudioDriver audioDriver;

audioDriver.init(AUDIO_SAMPLE_RATE_22050);

// Connects all sub-components in one call
aiClient.initAgent(
    "http://192.168.1.251:11434",  // Ollama URL
    "llama2",                       // LLM model
    "http://192.168.1.251:9000",   // Whisper STT URL
    "http://192.168.1.251:5000",   // Piper TTS URL
    &audioDriver                    // I2S speaker driver
);
```

### Streaming Callback

```cpp
ollamaClient->setStreamCallback([](const char* chunk) {
    // Handle streaming response chunks (e.g. update display)
    displayDriver.appendText(chunk);
});
```

## API Endpoints

### STT: Whisper Server — `POST /inference`

Send raw WAV audio and receive a transcript.

**Request:** `multipart/form-data`
```
field "file": audio.wav  (16-bit PCM, mono, 16 kHz)
```

**Response:**
```json
{ "text": "What is the weather today?" }
```

### STT: Google Cloud Speech-to-Text — `POST /v1/speech:recognize`

**Request:**
```json
{
    "config": {
        "encoding": "LINEAR16",
        "sampleRateHertz": 16000,
        "languageCode": "en-US"
    },
    "audio": {
        "content": "<base64-encoded WAV>"
    }
}
```

**Response:**
```json
{
    "results": [{
        "alternatives": [{ "transcript": "What is the weather today?" }]
    }]
}
```

### Ollama — `POST /api/chat`

Creates a chat completion from a prompt.

**Request:**
```json
{
    "model": "llama2",
    "stream": false,
    "messages": [
        {"role": "user", "content": "What is the weather today?"}
    ]
}
```

**Response:**
```json
{
    "model": "llama2",
    "created_at": "2024-01-01T00:00:00Z",
    "message": {
        "role": "assistant",
        "content": "I don't have real-time weather data, but I can help you find it!"
    },
    "done": true
}
```

### Ollama — `POST /api/chat` (Streaming)

Creates a streaming chat completion.

**Request:**
```json
{
    "model": "llama2",
    "stream": true,
    "messages": [
        {"role": "user", "content": "Hello"}
    ]
}
```

**Response (NDJSON, one object per line):**
```
{"model":"llama2","created_at":"...","message":{"role":"assistant","content":"Hello"}}
{"model":"llama2","created_at":"...","message":{"role":"assistant","content":" there"}}
{"model":"llama2","created_at":"...","message":{"role":"assistant","content":"!"},"done":true}
```

### Ollama — `GET /api/tags`

Lists available models.

**Response:**
```json
{
    "models": [
        {
            "name": "llama2",
            "modified_at": "2024-01-01T00:00:00Z",
            "size": 474688460,
            "digest": "sha256:abc123"
        }
    ]
}
```

### Ollama — `POST /api/generate`

Generates text (for non-chat, single-turn prompts).

**Response:**
```json
{
    "model": "llama2",
    "response": "Generated text...",
    "done": true
}
```

### Ollama — `POST /api/show`

Shows model information.

**Response:**
```json
{
    "model": "llama2",
    "modelfile": "FROM llama2",
    "template": "You are a helpful assistant.",
    "details": {
        "parameter_count": 4096,
        "quantization": "q4_0"
    }
}
```

### TTS: Piper Server — `POST /api/tts`

Convert text to WAV audio.

**Request:** `application/json`
```json
{ "text": "The weather is sunny today.", "voice": "en_US-lessac-medium" }
```

**Response:** `audio/wav` (raw WAV bytes, 22 050 Hz, 16-bit, mono)

### TTS: Google Cloud Text-to-Speech — `POST /v1/text:synthesize`

**Request:**
```json
{
    "input": { "text": "The weather is sunny today." },
    "voice": { "languageCode": "en-US", "name": "en-US-Standard-A" },
    "audioConfig": { "audioEncoding": "LINEAR16", "sampleRateHertz": 22050 }
}
```

**Response:**
```json
{ "audioContent": "<base64-encoded WAV>" }
```

## Usage Examples

### Full Voice Pipeline

```cpp
// Initialise all components
AudioDriver audio;
audio.init(AUDIO_SAMPLE_RATE_22050);

AIClient ai;
ai.initAgent(
    "http://192.168.1.251:11434",  // Ollama
    "llama2",
    "http://192.168.1.251:9000",   // Whisper STT
    "http://192.168.1.251:5000",   // Piper TTS
    &audio
);

// Capture audio from I2S microphone
int16_t pcmBuf[16000 * 3];  // 3 seconds at 16 kHz
int numSamples = voiceDriver.readPCM(pcmBuf, sizeof(pcmBuf) / sizeof(pcmBuf[0]));

// One call: PCM → STT → Ollama → TTS → speaker
std::string response;
esp_err_t err = ai.processVoiceAudio(pcmBuf, numSamples, 16000, response);

if (err == ESP_OK) {
    displayDriver.showText(response.c_str());  // also spoken by TTSClient
}
```

### Text-Only Query (No Microphone)

```cpp
std::string response;
ai.processAgentQuery("What time is it?", response);
printf("Response: %s\n", response.c_str());
```

### Basic Chat Completion (OllamaClient directly)

```cpp
OllamaClient ollama;
ollama.init();
ollama.setServerUrl("http://192.168.1.251:11434");
ollama.setModel("llama2");

std::string response;
esp_err_t err = ollama.createChatCompletion("What can you do?", response);
if (err == ESP_OK) {
    printf("Response: %s\n", response.c_str());
}
```

### Streaming Chat

```cpp
OllamaClient ollama;
ollama.init();
ollama.setServerUrl("http://192.168.1.251:11434");

ollama.setStreamCallback([](const char* chunk) {
    displayDriver.appendText(chunk);
});

ollama.createChatCompletionStream("Tell me a story", nullptr);
```

### STT Only

```cpp
STTClient stt;
stt.init();
stt.setProvider(STT_PROVIDER_WHISPER);
stt.setServerUrl("http://192.168.1.251:9000");
stt.setLanguage("en-US");

std::string transcript;
esp_err_t err = stt.transcribe(pcmBuf, numSamples, 16000, transcript);
if (err == ESP_OK) {
    printf("You said: %s\n", transcript.c_str());
}
```

### TTS Only

```cpp
TTSClient tts;
tts.init(&audioDriver);
tts.setProvider(TTS_PROVIDER_PIPER);
tts.setServerUrl("http://192.168.1.251:5000");
tts.setVoice("en_US-lessac-medium");

tts.speak("Hello! How can I help you today?");
```

### List Ollama Models

```cpp
OllamaClient ollama;
ollama.init();

std::vector<std::string> models;
ollama.listModels(models);

for (const auto& model : models) {
    printf("Available model: %s\n", model.c_str());
}
```

### Health Check

```cpp
OllamaClient ollama;
ollama.init();

if (ollama.healthCheck() == ESP_OK) {
    printf("Ollama server is running\n");
}
```

## Integration with AIClient

The full pipeline is encapsulated in `processVoiceAudio()`.  Legacy code that already
calls `processVoiceCommand(text)` continues to work — the text is forwarded directly
to Ollama and (if TTS is configured) the response is spoken automatically.

```cpp
esp_err_t AIClient::processVoiceAudio(const int16_t* pcmData,
                                      size_t         numSamples,
                                      int            sampleRate,
                                      std::string&   response)
{
    // Step 1: Speech-to-Text
    std::string transcript;
    esp_err_t err = m_stt->transcribe(pcmData, numSamples, sampleRate, transcript);
    if (err != ESP_OK) return err;

    // Step 2: LLM processing
    err = processAgentQuery(transcript.c_str(), response);
    if (err != ESP_OK) return err;

    // Step 3: Text-to-Speech
    if (m_tts) {
        m_tts->speak(response);
    }
    return ESP_OK;
}
```

## Dependencies

- ESP-IDF >= 4.1.0
- espressif/cjson >= 1.0.0 (for JSON parsing)
- FreeRTOS (for async operations and mutex)

## Server Setup (Local Network)

### Whisper STT Server

Install [faster-whisper-server](https://github.com/fedirz/faster-whisper-server) or
[whisper.cpp](https://github.com/ggerganov/whisper.cpp) with its built-in HTTP server:

```bash
# faster-whisper-server (Docker)
docker run --gpus all -p 9000:8000 fedirz/faster-whisper-server:latest-cuda

# whisper.cpp server (CPU-only example)
./server -m models/ggml-base.en.bin --host 0.0.0.0 --port 9000
```

### Ollama LLM Server

```bash
# Install
curl -fsSL https://ollama.com/install.sh | sh  # Linux
brew install ollama                             # macOS

# Run (binds to all interfaces for LAN access)
OLLAMA_HOST=0.0.0.0 ollama serve

# Pull recommended models
ollama pull llama2
ollama pull mistral
```

### Piper TTS Server

```bash
# Install piper and its HTTP wrapper
pip install piper-tts
pip install wyoming-piper

# Run wyoming-piper as HTTP server (port 5000)
python -m wyoming_piper \
    --piper /path/to/piper \
    --voice en_US-lessac-medium \
    --uri tcp://0.0.0.0:5000
```

## Future Enhancements

- [ ] Implement HTTP client body using `esp_http_client` (currently header-only)
- [ ] Add cJSON-based JSON parsing for response bodies
- [ ] Implement streaming response parser (SSE / NDJSON)
- [ ] On-device wake-word detection before STT request
- [ ] Automatic fallback to Google STT when Whisper server is unreachable
- [ ] Automatic fallback to eSpeak when Piper TTS server is unreachable
- [ ] Support for multiple simultaneous LLM requests

## Troubleshooting

### STT Server Not Found

1. Is the Whisper server running? `curl http://192.168.1.251:9000/`
2. Is the ESP32 on the same WiFi subnet?
3. Check firewall rules on the server host.

### Ollama Server Not Found

1. Is Ollama installed? `ollama --version`
2. Is the server running with LAN binding? `OLLAMA_HOST=0.0.0.0 ollama serve`
3. Is the URL correct? Default port: `11434`
4. Verify: `curl http://192.168.1.251:11434/api/tags`

### No Models Available

1. Pull a model: `ollama pull llama2`
2. Check available models: `ollama list`
3. Restart Ollama: `ollama serve`

### TTS Server Not Found

1. Is the Piper TTS server running? `curl http://192.168.1.251:5000/`
2. Check server logs for errors.
3. Verify the voice model file exists.

### Slow Responses

1. Use a smaller/quantised LLM model (`mistral` is faster than `llama2`)
2. Enable streaming for faster initial display update
3. Reduce conversation history length
4. Run the STT/TTS servers on GPU hardware if available

## Related Documentation

- [Agentic Roadmap](./AGENT_ROADMAP.md) - Roadmap for autonomous agent capabilities
- [Ollama Documentation](https://ollama.com/docs)
- [ESP-IDF HTTP Client Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html)