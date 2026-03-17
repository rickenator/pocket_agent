# Smart Agent — TODO

Findings from the full project audit. Items are grouped by priority. File paths and line numbers are given where applicable.

---

## 🔴 Critical Bugs

- [x] **Duplicate `__del__` in `ollama.py`** (`src/ai/ollama.py` lines 65–76)
  Second definition silently overwrites the first and lacks the try/except, making cleanup worse. Fixed — second definition removed.
- [x] **`run_continuous()` references `chunk` after loop** (`main.py` line 138)
  `self.ui.chat.add_message("assistant", chunk)` captures only the last chunk instead of the full response. Fixed — accumulate chunks into a list and join.
- [x] **Gemini API authentication is wrong** (`src/ai/gemini.py` line 29)
  Uses `"Authorization: Bearer <key>"` header, but Gemini REST API authenticates via `?key=<api_key>` query parameter. Fixed.
- [x] **`httpx.JSONResponse.content_to_json` does not exist** (`src/ai/gemini.py` line 61)
  `httpx.JSONResponse` is not a real class. Should be `json.loads(line)`. Fixed.
- [x] **`LocalLLMProxy.list_models()` calls async client synchronously** (`src/ai/ollama.py` line 94)
  `self.backend.client.get()` must be `await`ed on an `httpx.AsyncClient`. Fixed.
- [x] **`is_available()` declared sync in base, async in subclasses** (`src/ai/base.py` line 27)
  Abstract method must match the `async def` signature used by both backends. Fixed.
- [x] **`asyncio.run()` called inside already-running event loop** (`test_agent.py` line 309)
  `interactive_mode` is `async`; calling `asyncio.run(self.run_all_tests())` raises `RuntimeError`. Fixed — use `await`.
- [x] **`interactive_mode()` called without `await`** (`test_agent.py` lines 351–354)
  `runner.interactive_mode()` discards the coroutine silently. Fixed — add `await`.

---

## 🟠 High Priority Bugs

- [x] **Context ordering wrong in `ollama.py`** (`src/ai/ollama.py` lines 30–35)
  New user message was prepended before history, so the model sees the current query before prior context. Fixed — history is now prepended before the new message.
- [x] **Context ordering wrong in `gemini.py`** (`src/ai/gemini.py` lines 33–41)
  Same issue as above. Fixed.
- [x] **`SmartAgent.close()` does not clean up voice resources** (`main.py` lines 140–144)
  `MicrophoneManager`, `SpeechRecognizer`, and `TextToSpeech` are never closed. Fixed — call `speech.close()` and `tts.close()` if they exist.
- [x] **`os` imported but unused in `gemini.py`** (`src/ai/gemini.py` line 3)
  Removed.

---

## 🟡 Medium Priority Issues

- [x] **Docstring corruption in `test_agent.py`** (`test_agent.py` line 106)
  `"""Test 3: Code generation queryhaupt"""` — "queryhaupt" is a corruption. Fixed to `"""Test 3: Code generation query"""`.
- [x] **`settings.yaml` committed with placeholder API key** (`config/settings.yaml`)
  File contains `api_key: "YOUR_GEMINI_API_KEY"`. Only `settings.example.yaml` should be committed; `settings.yaml` should be in `.gitignore`. Fixed — removed from git tracking.
- [x] **`settings.yaml` sets `ai_backend: "gemini"` but README defaults to Ollama** (`config/settings.yaml`)
  Inconsistency between committed config and documentation. Fixed — `settings.yaml` removed from tracking.
- [x] **`settings.yaml` references model `glm-4.7-flash`** but docs say `llama2` or `mistral`
  Non-standard model name in committed config. Fixed — `settings.yaml` removed from tracking.
- [x] **Cyrillic corruption in `PROJECT_SUMMARY.md` line 22**
  `├──анныйvoice/` → `├── voice/`. Fixed.
- [x] **README placeholder URLs** (`README.md` lines 276–277 and line 81)
  `https://github.com/your-repo/...` replaced with `rickenator/pocket_agent`. Fixed. Clone URL also fixed.
- [x] **No error recovery in main loop** (`main.py`)
  If AI fails mid-stream, no retry or fallback logic. Fixed — added try/except around the AI chat loop in both `run()` and `run_continuous()`.
- [x] **No logging framework** — `print()` used throughout; replace with Python `logging` module for production. Fixed — replaced all `print()` calls with `logger.*()` and configured logging from config `app.log_level`.

---

## 🟢 Lower Priority / Housekeeping

- [x] **`__pycache__` directories committed** (`src/ai/__pycache__/`, `src/ui/__pycache__/`)
  Add `__pycache__/` and `*.pyc` to `.gitignore` and remove from tracking. Fixed — `.gitignore` already has the rule; files removed from tracking.
- [x] **`build/` directories committed** (`smart-agent/build/`, `smart-agent/esp32/build/`)
  Generated build artifacts should not be in version control. Fixed — removed from git tracking.
- [x] **`sdkconfig` and `sdkconfig.old` committed** (`smart-agent/sdkconfig`, `smart-agent/esp32/sdkconfig`, `smart-agent/esp32/sdkconfig.old`)
  Only `sdkconfig.defaults` should be committed. Fixed — removed from git tracking.
- [x] **`smart-agent/sdkconfig` at wrong level** — should only exist inside `smart-agent/esp32/`. Fixed — removed from git tracking.
- [x] **Duplicate `WIFI_PROVISIONING.md`** — `smart-agent/` and `smart-agent/esp32/` both have one. Consolidate or cross-link. Fixed — `smart-agent/WIFI_PROVISIONING.md` now redirects to `esp32/WIFI_PROVISIONING.md`.
- [x] **Duplicate `PROJECT_SUMMARY.md`** — same issue. Fixed — `smart-agent/esp32/PROJECT_SUMMARY.md` now redirects to `../PROJECT_SUMMARY.md`.
- [x] **Duplicate quickstart docs** — `docs/QUICK_START.md` and `esp32/QUICKSTART.md`. Consolidate. Fixed — `esp32/QUICKSTART.md` now redirects to `../docs/QUICK_START.md`.
- [x] **`smart-agent/CMakeLists.txt` duplicates `smart-agent/esp32/CMakeLists.txt`** — remove or consolidate. Fixed — `smart-agent/CMakeLists.txt` deleted.
- [x] **`base/home/` directory purpose undocumented** — stale developer-path artefact; removed from the repository.
- [x] **`esp-idf/` is a vendored full copy of ESP-IDF** — converted to a git submodule pointing to `espressif/esp-idf` at `v5.2.1`.
- [x] **No root-level `.gitignore`** — Created (see `.gitignore`).
- [x] **No root-level `README.md`** — Created (see `README.md`).
- [x] **README mentions `cp config/settings.example.yaml config/settings.yaml`** but `settings.yaml` is already committed — clarify instructions. Fixed — `settings.yaml` removed from tracking.

---

## 🔵 Missing / Future Enhancements

- [x] **`google-generativeai` not in `requirements.txt`** — add if using the official Python SDK for Gemini. Fixed — added `google-generativeai>=0.3.0`.
- [x] **`pyttsx3` not in `requirements.txt`** — add if using system TTS. Fixed — added `pyttsx3>=2.90`.
- [x] **No conversation history management** — `SmartAgent` now maintains a `conversation_history` list that is passed to AI backends on every call; `test_agent.py` interactive mode also tracks history with a `clear` command to reset it.
- [x] **No CI/CD pipeline** — added `.github/workflows/ci.yml` with `flake8` linting and `pytest` unit tests triggered on push and pull request.
- [x] **No `ARCHITECTURE.md`** — Created (see `ARCHITECTURE.md`).
- [x] **No `CHANGELOG.md`** — Created (see `CHANGELOG.md`).
- [x] **No `LICENSE` file** at repo root or `smart-agent/` level (only in `esp-idf/`). Fixed — MIT LICENSE file added at repo root.
- [x] **STT/TTS documentation** — Added full speech-to-response pipeline docs in `README.md`, `ARCHITECTURE.md`, `PROJECT_SUMMARY.md`, and `esp32/docs/AI_CLIENT.md`. Clarified that Ollama handles text only; STT (Whisper/Google) and TTS (Piper/Google) are separate services.
- [x] **ESP32 `stt_client` component** — `components/stt_client/stt_client.h` added. Supports Whisper HTTP server (multipart/form-data WAV upload) and Google Cloud Speech REST API. Includes retry logic and configurable timeout.
- [x] **ESP32 `tts_client` component** — `components/tts_client/tts_client.h` added. Supports Piper TTS HTTP server, eSpeak-ng HTTP, Google Cloud TTS, and ElevenLabs. Feeds WAV audio to `AudioDriver` for I2S speaker playback.
- [x] **`ai_client` STT/TTS pipeline** — `AIClient::initAgent()` extended to accept STT and TTS server URLs and an `AudioDriver` pointer. New `processVoiceAudio()` method chains STT → Ollama → TTS in a single call.
- [x] **Python `MockSpeechRecognizer`** — Added to `src/voice/speech.py` for unit tests that must run without a microphone or network connection.
- [x] **ESP32 `stt_client` / `tts_client` HTTP body implementation** — full `esp_http_client`-based implementations for Whisper, Google STT, Piper, Google TTS, ElevenLabs, and eSpeak complete.
- [x] **Wake-word detection** — `WakeWordDetector` class in `src/voice/speech.py`; configurable keywords (default: "hey buddy", "hey agent"); strips wake-word prefix before sending to LLM.
- [x] **Automatic STT fallback** — `SpeechRecognizer` tries Whisper HTTP first, falls back to Google STT when the local server is unreachable.
- [x] **Automatic TTS fallback** — `TextToSpeech` tries Piper HTTP first, falls back to system TTS (espeak/say/pyttsx3) when the local server is unreachable.
- [ ] **Calendar integration**
- [ ] **Weather services**
- [ ] **Smart home control**
- [ ] **Web interface**
- [ ] **Mobile companion app**
- [ ] **Multi-language support**
- [ ] **Advanced TTS customization**
