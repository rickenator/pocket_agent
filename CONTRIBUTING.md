# Contributing to Pocket Agent

Thank you for your interest in contributing! This guide explains how to get
involved, what standards we follow, and how to submit your work.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Development Setup](#development-setup)
3. [Coding Standards](#coding-standards)
4. [Pull Request Process](#pull-request-process)
5. [Reporting Bugs](#reporting-bugs)
6. [Requesting Features](#requesting-features)
7. [Code of Conduct](#code-of-conduct)

---

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/<your-username>/pocket_agent.git
   cd pocket_agent
   ```
3. Create a **feature branch** from `main`:
   ```bash
   git checkout -b feature/my-improvement
   ```

---

## Development Setup

### Python Agent (`smart-agent/`)

```bash
cd smart-agent
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
pip install flake8 pytest pytest-asyncio
```

Copy the example config and set your preferences:
```bash
cp config/settings.example.yaml config/settings.yaml
# Edit config/settings.yaml — never commit this file
```

Run the test suite:
```bash
pytest tests/ -v
```

### ESP32 Firmware (`smart-agent/esp32/`)

Install ESP-IDF v5.x and activate the environment, then:
```bash
cd smart-agent/esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

See [`smart-agent/docs/ESP32_SETUP.md`](smart-agent/docs/ESP32_SETUP.md) for
full hardware prerequisites and flashing instructions.

---

## Coding Standards

### Python

| Rule | Detail |
|------|--------|
| **Style** | [PEP 8](https://peps.python.org/pep-0008/), enforced by `flake8` |
| **Max line length** | 120 characters |
| **Imports** | Standard library → third-party → local; one import per line |
| **Docstrings** | Module, class, and public function docstrings required |
| **Logging** | Use the `logging` module — no bare `print()` statements |
| **Async** | Prefer `async`/`await`; do not call `asyncio.run()` inside a running loop |
| **Type hints** | Encouraged for all new public functions |

Run the linter before pushing (same settings used by CI):
```bash
flake8 src/ main.py test_agent.py \
  --count --select=E9,F63,F7,F82 --show-source --statistics
flake8 src/ main.py test_agent.py \
  --count --max-line-length=120 --statistics --exit-zero
```

> The CI workflow (`.github/workflows/python-app.yml`) uses the same flags.

### C / ESP32 Firmware

- Follow the [ESP-IDF coding style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html).
- Use `snake_case` for functions and variables; `UPPER_CASE` for macros.
- Every component must have a `CMakeLists.txt` that lists its sources explicitly.
- Check return values from ESP-IDF APIs; use `ESP_ERROR_CHECK` or log errors.

---

## Pull Request Process

1. **Keep PRs focused** — one logical change per PR makes review faster.
2. **Update documentation** — if your change affects behaviour described in
   `README.md`, `ARCHITECTURE.md`, or any file under `docs/`, update those
   files in the same PR.
3. **Pass CI** — the GitHub Actions workflow must be green before merging.
4. **Describe your change** — fill in the PR template with:
   - *What* changed and *why*
   - How to test it manually
   - Any known limitations or follow-up work
5. **Request a review** — tag a maintainer or leave the PR open for the community.

Maintainers aim to review PRs within **7 days**. Feel free to ping if you
haven't heard back after that.

---

## Reporting Bugs

Search [existing issues](https://github.com/rickenator/pocket_agent/issues) first.
If yours is new, open an issue and include:

- **Environment**: OS, Python version, ESP-IDF version (if applicable)
- **Steps to reproduce** the problem
- **Expected vs. actual** behaviour
- Relevant **log output** or screenshots

---

## Requesting Features

Open a [GitHub Issue](https://github.com/rickenator/pocket_agent/issues) with
the label `enhancement`. Describe:

- The use case that motivates the feature
- A rough sketch of the API or UX you have in mind
- Whether you are willing to implement it yourself

Large features (new AI backends, new hardware support) are best discussed in an
issue *before* a PR is opened, so that design decisions can be agreed upon early.

---

## Code of Conduct

This project follows the
[Contributor Covenant v2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).
In short: be respectful, inclusive, and constructive. Harassment or
discrimination of any kind will not be tolerated.

---

*Thank you for helping make Pocket Agent better!* 🤖
