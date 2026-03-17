# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **ESP32 `stt_client.cpp`** — full `esp_http_client`-based implementation of `STTClient`:
  - Whisper provider: multipart/form-data WAV upload to a faster-whisper-server endpoint.
  - Google Cloud STT provider: JSON POST with base-64 encoded LINEAR16 PCM audio.
  - WAV header builder, base-64 encoder, JSON response parsers.
  - Retry logic with exponential back-off; configurable timeout and retries.
- **ESP32 `tts_client.cpp`** — full `esp_http_client`-based implementation of `TTSClient`:
  - Piper TTS provider: JSON POST to `/api/tts`, receives raw WAV bytes.
  - Google Cloud TTS provider: JSON POST, decodes base-64 `audioContent`.
  - ElevenLabs provider: JSON POST with `xi-api-key` header, receives MP3/PCM.
  - eSpeak-ng provider: delegates to the Piper-compatible REST interface.
  - Base-64 decoder, streaming chunk callback, WAV-header skipping before playback.
  - Retry logic with exponential back-off on all providers.
- **`AIClient::initAgent()` extended** — now accepts optional `sttUrl`, `ttsUrl`, and
  `AudioDriver*` parameters, instantiates `STTClient` and `TTSClient` when provided.
- **`AIClient::processVoiceAudio()`** — connects the full PCM → STT → Ollama → TTS
  pipeline in a single call; TTS playback is non-blocking.
- **`WakeWordDetector`** (`src/voice/speech.py`) — lightweight keyword spotter;
  configurable wake-words (default: "hey buddy", "hey agent", "ok agent"); strips the
  wake-word prefix before passing the utterance to the LLM.
- **`WhisperHTTPClient`** (`src/voice/speech.py`) — thin synchronous wrapper around a
  faster-whisper-server HTTP endpoint; raises `RuntimeError` on failure to enable fallback.
- **Automatic STT fallback** (`src/voice/speech.py`) — `SpeechRecognizer` tries the
  local Whisper HTTP server first and falls back to Google Cloud Speech when the server
  is unreachable.
- **`PiperHTTPClient`** (`src/voice/tts.py`) — thin synchronous wrapper around a
  Piper TTS HTTP server; raises `RuntimeError` on failure to enable fallback.
- **Automatic TTS fallback** (`src/voice/tts.py`) — `TextToSpeech` tries the local Piper
  HTTP server first and falls back to the OS-native TTS engine (macOS `say`, Linux
  `espeak`, Windows `pyttsx3`) when the server is unreachable.
- **AGENT_ROADMAP.md updated** — capability status table refreshed to reflect all
  implemented components; task checklists for Phases 1 and 2 updated with completed items.
- Conversation history management — `SmartAgent` now maintains session context across
  turns and passes it to AI backends on every call so the model sees full conversation
  history in order.
- Interactive mode in `test_agent.py` now tracks conversation history and provides a
  `clear` command to reset it between sessions.
- GitHub Actions CI workflow (`.github/workflows/ci.yml`) — runs `flake8` linting and
  `pytest` unit tests on every push and pull request.
- Root-level `README.md` and `.gitignore`.
- `ARCHITECTURE.md` describing the system design and component interactions.
- MIT `LICENSE` file at the repo root.
- `google-generativeai` and `pyttsx3` added to `requirements.txt`.
- `WIFI_PROVISIONING.md` cross-linked between `smart-agent/` and `smart-agent/esp32/`.

### Fixed
- Duplicate `__del__` in `ollama.py` — second definition removed.
- `run_continuous()` response accumulation — chunks now collected into a list and joined.
- Gemini API authentication — switched from `Authorization: Bearer` header to
  `?key=<api_key>` query parameter.
- `httpx.JSONResponse.content_to_json` replaced with `json.loads(line)`.
- `LocalLLMProxy.list_models()` now correctly `await`s the async HTTP client.
- `is_available()` abstract method signature made `async def` in the base class.
- `asyncio.run()` inside a running event loop in `test_agent.py` — replaced with `await`.
- Context ordering in `ollama.py` and `gemini.py` — history is now prepended before
  the new user message.
- `SmartAgent.close()` now cleans up voice and AI resources.
- Removed unused `os` import from `gemini.py`.
- Docstring corruption in `test_agent.py` Test 3 description fixed.
- `settings.yaml` removed from git tracking (contains placeholder API keys).
- Cyrillic corruption in `PROJECT_SUMMARY.md` fixed.
- README placeholder URLs updated to `rickenator/pocket_agent`.
- Error recovery added to main loop with try/except around AI chat calls.
- Replaced `print()` throughout with Python `logging` module calls.
- `__pycache__` directories and build artefacts removed from tracking.
- `sdkconfig` / `sdkconfig.old` files removed from tracking.
- Duplicate `CMakeLists.txt` at `smart-agent/` level removed.
- `base/home/` stale developer artefact directory removed.

### Changed
- `esp-idf` converted from vendored copy to a proper git submodule.
