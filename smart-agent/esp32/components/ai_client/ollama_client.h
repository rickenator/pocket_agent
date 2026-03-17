#ifndef OLLAMA_CLIENT_H
#define OLLAMA_CLIENT_H

#include <stdint.h>
#include <string>
#include <vector>
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Ollama model types
typedef enum {
    OLLAMA_MODEL_UNKNOWN = 0,
    OLLAMA_MODEL_LLAMA2,
    OLLAMA_MODEL_LLAMA3,
    OLLAMA_MODEL_MISTRAL,
    OLLAMA_MODEL_GEMMA,
    OLLAMA_MODEL_CUSTOM
} ollama_model_t;

// Streaming callback — called for each content chunk received
typedef void (*ollama_stream_callback_t)(const char* chunk);

// Internal buffer used by the HTTP event handler
struct OllamaResponseBuffer {
    char*  data;
    size_t size;
    size_t capacity;
    ollama_stream_callback_t streamCallback; // non-null means streaming mode
    // Full accumulated assistant content (streaming mode only, for history updates)
    char*  fullContent;
    size_t fullContentSize;
    size_t fullContentCapacity;
};

// Conversation message (role + content)
struct OllamaMessage {
    std::string role;    // "system", "user", "assistant", "tool"
    std::string content;
};

// Ollama client for making API calls to a local Ollama server
class OllamaClient {
public:
    OllamaClient();
    ~OllamaClient();

    esp_err_t init();
    void deinit();

    // Configuration
    void setServerUrl(const char* url);
    void setModel(const char* model);
    void setStreamCallback(ollama_stream_callback_t callback);
    void setTimeout(int timeout_ms);
    void setMaxRetries(int retries);

    // Conversation history management
    void addSystemPrompt(const char* prompt);
    void addToHistory(const char* role, const char* content);
    void clearHistory();
    const std::vector<OllamaMessage>& getHistory() const;

    // Non-streaming chat completion (POST /api/chat, stream:false)
    // Uses full conversation history + prompt as a new user turn.
    esp_err_t createChatCompletion(const char* prompt, std::string& response);

    // Streaming chat completion (POST /api/chat, stream:true)
    // Calls callback for each content chunk; uses full conversation history.
    esp_err_t createChatCompletionStream(const char* prompt, ollama_stream_callback_t callback);

    // List available models (GET /api/tags)
    esp_err_t listModels(std::vector<std::string>& models);

    // Show model details (POST /api/show)
    esp_err_t showModel(const char* model, std::string& info);

    // Health check — GET / or /api/tags, returns ESP_OK on HTTP 200
    esp_err_t healthCheck();

private:
    std::string m_serverUrl;
    std::string m_model;
    ollama_stream_callback_t m_streamCallback;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;
    int m_timeoutMs;
    int m_maxRetries;

    std::vector<OllamaMessage> m_conversationHistory;

    static const int DEFAULT_TIMEOUT_MS  = 30000;
    static const int DEFAULT_MAX_RETRIES = 3;

    // HTTP helpers
    esp_err_t sendRequest(const char* endpoint, const char* payload,
                          std::string& response, bool isGet = false);
    esp_err_t sendRequestStreaming(const char* endpoint, const char* payload,
                                   ollama_stream_callback_t callback);
    std::string buildUrl(const char* endpoint) const;

    // Build the messages JSON array from history + new user turn
    std::string buildMessagesJson(const char* userPrompt) const;

    // HTTP event handler (static, registered with esp_http_client)
    static esp_err_t httpEventHandler(esp_http_client_event_t* evt);
};

#endif // OLLAMA_CLIENT_H