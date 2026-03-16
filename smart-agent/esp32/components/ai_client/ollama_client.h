#ifndef OLLAMA_CLIENT_H
#define OLLAMA_CLIENT_H

#include <stdint.h>
#include <string>
#include "esp_err.h"
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

// Streaming callback
typedef void (*ollama_stream_callback_t)(const char* chunk);

// Ollama client for making API calls to local Ollama server
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

    // API calls
    esp_err_t createChatCompletion(const char* prompt, std::string& response);
    esp_err_t listModels(std::vector<std::string>& models);
    esp_err_t generateImage(const char* prompt, std::string& response);
    esp_err_t showModel(const char* model, std::string& info);

    // Health check
    esp_err_t healthCheck();

    // Streaming API
    esp_err_t createChatCompletionStream(const char* prompt, ollama_stream_callback_t callback);

private:
    std::string m_serverUrl;
    std::string m_model;
    ollama_stream_callback_t m_streamCallback;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;

    // Internal helpers
    esp_err_t sendRequest(const char* endpoint, const char* payload, std::string& response);
    std::string getBaseUrl() const;
};

#endif // OLLAMA_CLIENT_H