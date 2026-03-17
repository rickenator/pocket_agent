# Pocket Agent — Project Roadmap

This file tracks the planned features and future directions for **Pocket Agent**.
All critical bugs and infrastructure items have been resolved; what remains are
enhancements that will make the project even more capable.

See [`smart-agent/TODO.md`](smart-agent/TODO.md) for the full historical audit log
of completed fixes.

---

## 🔷 Planned Features

### Voice & Language
- [ ] **Wake-word customization** — Let users configure their own wake phrase via `config/settings.yaml`
- [ ] **Multi-language support** — STT/TTS language selection per-session
- [ ] **Advanced TTS customization** — Voice, speed, and pitch controls for Piper TTS

### AI & Intelligence
- [ ] **Calendar integration** — Query and create calendar events through voice commands
- [ ] **Weather services** — Real-time weather lookup via open APIs
- [ ] **Smart home control** — Home Assistant / MQTT bridge for device control
- [ ] **Tool/function calling** — Structured tool-use loop for the Ollama backend

### Connectivity & Interfaces
- [ ] **Web interface** — Browser-based chat UI served from the ESP32 or Python host
- [ ] **Mobile companion app** — iOS / Android companion for remote control and monitoring
- [ ] **REST API** — Expose the agent's capabilities as a local HTTP API

### Hardware & Firmware
- [ ] **Touch sensor driver** — Full touch-input support for the Waveshare AMOLED panel
- [ ] **OTA firmware updates** — Over-the-air ESP32 firmware upgrade support
- [ ] **Low-power mode** — Deep-sleep between voice interactions to extend battery life

---

## 🔵 Infrastructure / Quality

- [ ] **Unit test coverage** — Add `tests/` directory with pytest unit tests for AI backends and voice modules
- [ ] **Integration tests** — End-to-end test harness using `MockSpeechRecognizer`
- [ ] **Pre-commit hooks** — `flake8` and `isort` hooks via `pre-commit`
- [ ] **Type annotations** — Progressively add `mypy`-compatible type hints across `src/`

---

*Items are not ordered by priority. Open an issue or PR to discuss any of these!*
