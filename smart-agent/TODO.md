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
- [ ] **`settings.yaml` committed with placeholder API key** (`config/settings.yaml`)
  File contains `api_key: "YOUR_GEMINI_API_KEY"`. Only `settings.example.yaml` should be committed; `settings.yaml` should be in `.gitignore`.
- [ ] **`settings.yaml` sets `ai_backend: "gemini"` but README defaults to Ollama** (`config/settings.yaml`)
  Inconsistency between committed config and documentation. Align config default with Ollama as documented.
- [ ] **`settings.yaml` references model `glm-4.7-flash`** but docs say `llama2` or `mistral`
  Non-standard model name in committed config; update to match documented defaults.
- [x] **Cyrillic corruption in `PROJECT_SUMMARY.md` line 22**
  `├──анныйvoice/` → `├── voice/`. Fixed.
- [x] **README placeholder URLs** (`README.md` lines 276–277 and line 81)
  `https://github.com/your-repo/...` replaced with `rickenator/pocket_agent`. Fixed. Clone URL also fixed.
- [ ] **No error recovery in main loop** (`main.py`)
  If AI fails mid-stream, no retry or fallback logic. Add try/except around the AI chat loop.
- [ ] **No logging framework** — `print()` used throughout; replace with Python `logging` module for production.

---

## 🟢 Lower Priority / Housekeeping

- [ ] **`__pycache__` directories committed** (`src/ai/__pycache__/`, `src/ui/__pycache__/`)
  Add `__pycache__/` and `*.pyc` to `.gitignore` and remove from tracking.
- [ ] **`build/` directories committed** (`smart-agent/build/`, `smart-agent/esp32/build/`)
  Generated build artifacts should not be in version control.
- [ ] **`sdkconfig` and `sdkconfig.old` committed** (`smart-agent/sdkconfig`, `smart-agent/esp32/sdkconfig`, `smart-agent/esp32/sdkconfig.old`)
  Only `sdkconfig.defaults` should be committed.
- [ ] **`smart-agent/sdkconfig` at wrong level** — should only exist inside `smart-agent/esp32/`.
- [ ] **Duplicate `WIFI_PROVISIONING.md`** — `smart-agent/` and `smart-agent/esp32/` both have one. Consolidate or cross-link.
- [ ] **Duplicate `PROJECT_SUMMARY.md`** — same issue.
- [ ] **Duplicate quickstart docs** — `docs/QUICK_START.md` and `esp32/QUICKSTART.md`. Consolidate.
- [ ] **`smart-agent/CMakeLists.txt` duplicates `smart-agent/esp32/CMakeLists.txt`** — remove or consolidate.
- [ ] **`base/home/` directory purpose undocumented** — document or remove.
- [ ] **`esp-idf/` is a vendored full copy of ESP-IDF** — should be a git submodule or rely on the system ESP-IDF installation.
- [ ] **No root-level `.gitignore`** — Created (see `.gitignore`).
- [ ] **No root-level `README.md`** — Created (see `README.md`).
- [ ] **README mentions `cp config/settings.example.yaml config/settings.yaml`** but `settings.yaml` is already committed — clarify instructions.

---

## 🔵 Missing / Future Enhancements

- [ ] **`google-generativeai` not in `requirements.txt`** — add if using the official Python SDK for Gemini.
- [ ] **`pyttsx3` not in `requirements.txt`** — add if using system TTS.
- [ ] **No conversation history management** — neither Ollama nor Gemini backends persist conversation state between calls. Implement session context management.
- [ ] **No CI/CD pipeline** — add GitHub Actions for linting and tests.
- [ ] **No `ARCHITECTURE.md`** — Created (see `ARCHITECTURE.md`).
- [ ] **No `CHANGELOG.md`** — create when releases begin.
- [ ] **No `LICENSE` file** at repo root or `smart-agent/` level (only in `esp-idf/`).
- [ ] **Calendar integration**
- [ ] **Weather services**
- [ ] **Smart home control**
- [ ] **Web interface**
- [ ] **Mobile companion app**
- [ ] **Multi-language support**
- [ ] **Advanced TTS customization**
