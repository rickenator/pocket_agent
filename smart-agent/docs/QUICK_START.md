# Quick Start Guide

Get your Smart Agent up and running in minutes!

## Option 1: Ollama (Local, Free)

### Step 1: Install Ollama
```bash
# macOS
brew install ollama

# Linux
curl -fsSL https://ollama.com/install.sh | sh

# Windows
# Download from https://ollama.com
```

### Step 2: Start Ollama
```bash
ollama serve
# Keep this running in a terminal
```

### Step 3: Choose a Model
```bash
ollama pull llama2
ollama pull mistral
ollama pull codellama
```

### Step 4: Configure the Agent
```bash
cd smart-agent
cp config/settings.example.yaml config/settings.yaml

# Edit config/settings.yaml
# Change ai_backend to "ollama"
# Set ollama.model to your preferred model
```

### Step 5: Run the Agent
```bash
python main.py
```

## Option 2: Gemini (Cloud, Free Tier)

### Step 1: Get API Key
1. Go to [Google AI Studio](https://ai.google.dev/)
2. Create a free API key
3. Note the API key

### Step 2: Configure the Agent
```bash
cd smart-agent
cp config/settings.example.yaml config/settings.yaml

# Edit config/settings.yaml
# Change ai_backend to "gemini"
# Set gemini.api_key to your API key
```

### Step 3: Run the Agent
```bash
python main.py
```

## Option 3: ESP32 Firmware

### Step 1: Setup ESP-IDF
```bash
# Linux工作开展
git clone --recursive https://github.com/espressif/ESP-IDF.git
cd ESP-IDF
./install.sh esp32s3
source export.sh
```

### Step 2: Clone Display Driver
```bash
git clone https://github.com/waveshare/AMOLED_Display-466x466-EPD
```

### Step 3: Build and Flash
```bash
cd AMOLED_Display-466x466-EPD
idf.py build flash monitor
```

## Voice Input

### Test Microphone
```bash
# Test microphone setup
python -c "from src.voice.mic import MicrophoneManager; m = MicrophoneManager({'sample_rate': 16000, 'chunk_size': 1024}); m.setup(); m.start_recording()"
```

### Test Speech Recognition
```bash
# Test speech-to-text
python -c "from src.voice.speech import SpeechRecognizer; from src.voice.mic import MicrophoneManager; m = MicrophoneManager({'sample_rate': 16000}); m.setup(); sr = SpeechRecognizer({'sample_rate': 16000}, m); print(sr.listen())"
```

## Troubleshooting

### No microphone found
```bash
# List available devices
python -c "import pyaudio; p = pyaudio.PyAudio(); [print(p.get_device_info_by_index(i)['name']) for i in range(p.get_device_count())]"
```

### Ollama not responding
```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags
# Should return list of models
```

### Display not working
- Check connections
- Verify SPI pins are correct
- Try running in PC simulation mode first

## Advanced Usage

### Continuous Listening Mode
```bash
python main.py --continuous
```

### Custom AI Model
Edit `config/settings.yaml`:
```yaml
ollama:
  model: "your-custom-model-name"
```

### Adjust Voice Parameters
Edit `config/settings.yaml`:
```yaml
voice:
  energy_threshold: 1000  # Adjust for your environment
  silence_timeout: 3  # Seconds before stopping
```

## Next Steps

1. Customize the UI with your own design
2. Add more features (calendar, weather, etc.)
3. Integrate with other services
4. Optimize for your specific use case

Happy building! 🚀