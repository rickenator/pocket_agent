# AI Client Documentation

## Overview

The Smart Agent AI Client provides intelligent language processing capabilities through integration with local LLMs (Large Language Models). This client is designed to work with Ollama, a local LLM server, enabling offline AI capabilities.

## Architecture

```
┌─────────────────┐
│  Voice Driver   │
│  (Speech Input) │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  AI Client      │◄──────────────────┐
│  - Process Cmd  │                   │
│  - Query LLM    │                   │
└────────┬────────┘                   │
         │                            │
         ▼                            │
┌─────────────────┐                   │
│  Ollama Client  │◄──────────────────┘
│  - HTTP API     │   Local Ollama
│  - Streaming    │   Server (localhost:11434)
└─────────────────┘
```

## Components

### AIClient

The main AI client interface for high-level interactions.

**Features:**
- Voice command processing
- Text query processing
- Response handling
- Response type classification

**Status:** Simulation mode (ready for integration)

### OllamaClient

The Ollama client for direct API interactions with a local LLM server.

**Features:**
- Chat completion API
- Streaming responses
- Model listing
- Image generation
- Model info retrieval
- Health checks

**Status:** Core implementation (HTTP client pending)

## Configuration

### Server Configuration

```cpp
ollamaClient->setServerUrl("http://127.0.0.1:11434");
ollamaClient->setModel("llama2");  // or "llama3", "mistral", etc.
```

### Streaming Callback

```cpp
ollamaClient->setStreamCallback([](const char* chunk) {
    // Handle streaming response chunks
    printf("Stream: %s", chunk);
});
```

## API Endpoints

### /api/chat

Creates a chat completion from a prompt.

**Request:**
```json
{
    "model": "llama2",
    "stream": false,
    "messages": [
        {"role": "user", "content": "Hello"}
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
        "content": "Hello! How can I help you?"
    },
    "done": true
}
```

### /api/chat (Streaming)

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

**Response (SSE format):**
```
data: {"model":"llama2","created_at":"...","message":{"role":"assistant","content":"Hello"}}

data: {"model":"llama2","created_at":"...","message":{"role":"assistant","content":" "}}

data: {"model":"llama2","created_at":"...","message":{"role":"assistant","content":"!"}}

data: [DONE]
```

### /api/tags

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

### /api/generate

Generates text (for image prompts, etc.).

**Response:**
```json
{
    "model": "llama2",
    "response": "Generated text...",
    "done": true
}
```

### /api/show

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

## Usage Examples

### Basic Chat Completion

```cpp
OllamaClient ollama;
ollama.init();

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

ollama.setStreamCallback([](const char* chunk) {
    printf("Chunk: %s", chunk);
});

ollama.createChatCompletionStream("Tell me a story", nullptr);
```

### List Models

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

To integrate Ollama with the main AIClient:

```cpp
class AIClient {
public:
    esp_err_t processVoiceCommand(const char* command);
    esp_err_t processTextQuery(const char* query);

private:
    OllamaClient m_ollama;
};

esp_err_t AIClient::processVoiceCommand(const char* command) {
    std::string response;

    // Initialize Ollama
    if (m_ollama.init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Ollama");
        return ESP_FAIL;
    }

    // Set configuration
    m_ollama.setModel("llama2");
    m_ollama.setStreamCallback([](const char* chunk) {
        // Store chunk for display
    });

    // Get response
    esp_err_t err = m_ollama.createChatCompletion(command, response);

    if (err == ESP_OK) {
        strncpy(m_response, response.c_str(), sizeof(m_response) - 1);
        m_responseType = AI_RESPONSE_TEXT;
    }

    return err;
}
```

## Dependencies

- ESP-IDF >= 4.1.0
- espressif/cjson >= 1.0.0 (for JSON parsing)
- FreeRTOS (for async operations and mutex)

## Future Enhancements

- [ ] Implement proper HTTP client using esp_http_client
- [ ] Add JSON parsing using cJSON library
- [ ] Implement streaming response parser (SSE format)
- [ ] Add retry logic and timeout handling
- [ ] Support for custom models
- [ ] Add conversation history/memory
- [ ] Implement prompt templates
- [ ] Add request/response logging
- [ ] Support for multiple simultaneous requests

## Troubleshooting

### Server Not Found

If the Ollama server is not running, check:
1. Is Ollama installed? (`ollama --version`)
2. Is the server running? (`ollama serve`)
3. Is the URL correct? (default: `http://127.0.0.1:11434`)
4. Is the port open? (default: `11434`)

### No Models Available

If no models are found:
1. Pull a model: `ollama pull llama2`
2. Check available models: `ollama list`
3. Verify server is serving models: `curl http://127.0.0.1:11434/api/tags`

### Slow Responses

Optimizations:
1. Use a smaller model (e.g., `llama2` instead of `llama3`)
2. Enable streaming for faster initial response
3. Reduce context length
4. Use quantized models (already done by default)

## Related Documentation

- [Agentic Roadmap](./AGENT_ROADMAP.md) - Roadmap for autonomous agent capabilities
- [Ollama Documentation](https://ollama.com/docs)
- [ESP-IDF HTTP Client Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html)