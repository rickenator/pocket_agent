#include "ollama_client.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include "esp_log.h"

static const char* TAG = "OLLAMA_CLIENT";

OllamaClient::OllamaClient()
    : m_streamCallback(nullptr)
    , m_mutex(nullptr)
    , m_initialized(false)
{
    // Default to localhost:11434
    m_serverUrl = "http://127.0.0.1:11434";
    m_model = "llama2";
}

OllamaClient::~OllamaClient()
{
    deinit();
}

esp_err_t OllamaClient::init()
{
    if (m_initialized) {
        return ESP_OK;
    }

    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "Ollama client initialized");
    return ESP_OK;
}

void OllamaClient::deinit()
{
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    m_initialized = false;
}

void OllamaClient::setServerUrl(const char* url)
{
    if (m_initialized && url) {
        m_serverUrl = url;
        ESP_LOGI(TAG, "Server URL set to: %s", url);
    }
}

void OllamaClient::setModel(const char* model)
{
    if (m_initialized && model) {
        m_model = model;
        ESP_LOGI(TAG, "Model set to: %s", model);
    }
}

void OllamaClient::setStreamCallback(ollama_stream_callback_t callback)
{
    if (m_initialized) {
        m_streamCallback = callback;
        ESP_LOGI(TAG, "Stream callback set");
    }
}

std::string OllamaClient::getBaseUrl() const
{
    std::string url = m_serverUrl;
    // Ensure URL ends with /
    if (url.back() != '/') {
        url += "/";
    }
    return url;
}

esp_err_t OllamaClient::sendRequest(const char* endpoint, const char* payload, std::string& response)
{
    if (!m_initialized || !payload) {
        return ESP_ERR_INVALID_ARG;
    }

    // In a real implementation, you would use esp_http_client here
    // For simulation, we'll return a response

    ESP_LOGI(TAG, "Sending request to: %s", endpoint);

    // Parse JSON payload for simulation
    // Real implementation would use cJSON for proper JSON handling

    // Simulated response
    if (strcmp(endpoint, "/api/chat") == 0) {
        response = R"({
            "model": "llama2",
            "created_at": "2024-01-01T00:00:00Z",
            "message": {
                "role": "assistant",
                "content": "This is a simulated response from the Ollama API. In a real implementation, this would contain actual AI-generated text."
            },
            "done": true
        })";
    } else if (strcmp(endpoint, "/api/tags") == 0) {
        response = R"({
            "models": [
                {
                    "name": "llama2",
                    "modified_at": "2024-01-01T00:00:00Z",
                    "size": 474688460,
                    "digest": "sha256:abc123"
                },
                {
                    "name": "llama3",
                    "modified_at": "2024-01-01T00:00:00Z",
                    "size": 466568665,
                    "digest": "sha256:def456"
                }
            ]
        })";
    } else if (strcmp(endpoint, "/api/generate") == 0) {
        response = R"({
            "model": "llama2",
            "response": "This is a simulated generation response.",
            "done": true
        })";
    } else if (strcmp(endpoint, "/api/show") == 0) {
        response = R"({
            "model": "llama2",
            "modelfile": "FROM llama2",
            "template": "You are a helpful assistant.",
            "details": {
                "parameter_count": 4096,
                "quantization": "q4_0"
            }
        })";
    } else {
        ESP_LOGE(TAG, "Unknown endpoint: %s", endpoint);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

esp_err_t OllamaClient::createChatCompletion(const char* prompt, std::string& response)
{
    if (!m_initialized || !prompt) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Creating chat completion with prompt: %s", prompt);

    // Construct payload
    std::stringstream payload;
    payload << "{";
    payload << "\"model\":\"" << m_model << "\",";
    payload << "\"stream\":false,";
    payload << "\"messages\":[";
    payload << "{\"role\":\"user\",\"content\":\"" << prompt << "\"}";
    payload << "]";
    payload << "}";

    // Send request
    std::string base = getBaseUrl();
    std::string endpoint = base + "api/chat";
    std::string payload_str = payload.str();

    ESP_LOGI(TAG, "Payload: %s", payload_str.c_str());

    // TODO: Replace with actual HTTP client implementation
    // esp_http_client_start, esp_http_client_set_method, esp_http_client_write, etc.

    esp_err_t ret = sendRequest("/api/chat", payload_str.c_str(), response);

    xSemaphoreGive(m_mutex);

    return ret;
}

esp_err_t OllamaClient::createChatCompletionStream(const char* prompt, ollama_stream_callback_t callback)
{
    if (!m_initialized || !prompt || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    m_streamCallback = callback;
    ESP_LOGI(TAG, "Creating chat completion stream with prompt: %s", prompt);

    // Construct payload
    std::stringstream payload;
    payload << "{";
    payload << "\"model\":\"" << m_model << "\",";
    payload << "\"stream\":true,";
    payload << "\"messages\":[";
    payload << "{\"role\":\"user\",\"content\":\"" << prompt << "\"}";
    payload << "]";
    payload << "}";

    std::string base = getBaseUrl();
    std::string endpoint = base + "api/chat";
    std::string payload_str = payload.str();

    ESP_LOGI(TAG, "Stream payload: %s", payload_str.c_str());

    // TODO: Implement streaming response handling
    // Parse SSE (Server-Sent Events) format and call callback for each chunk

    xSemaphoreGive(m_mutex);

    return ESP_OK;
}

esp_err_t OllamaClient::listModels(std::vector<std::string>& models)
{
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Listing models");

    std::string base = getBaseUrl();
    std::string response;

    esp_err_t ret = sendRequest("/api/tags", nullptr, response);

    xSemaphoreGive(m_mutex);

    if (ret == ESP_OK) {
        // Parse response to extract model names
        // TODO: Implement proper JSON parsing
        ESP_LOGI(TAG, "Available models: %s", response.c_str());
        models.push_back("llama2");
        models.push_back("llama3");
        models.push_back("mistral");
    }

    return ret;
}

esp_err_t OllamaClient::generateImage(const char* prompt, std::string& response)
{
    if (!m_initialized || !prompt) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Generating image with prompt: %s", prompt);

    std::string base = getBaseUrl();
    std::string response_str;

    esp_err_t ret = sendRequest("/api/generate", nullptr, response_str);

    xSemaphoreGive(m_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Image generation response: %s", response_str.c_str());
    }

    return ret;
}

esp_err_t OllamaClient::showModel(const char* model, std::string& info)
{
    if (!m_initialized || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Showing model info: %s", model);

    std::string base = getBaseUrl();
    std::string response_str;

    esp_err_t ret = sendRequest("/api/show", nullptr, response_str);

    xSemaphoreGive(m_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Model info: %s", response_str.c_str());
    }

    return ret;
}

esp_err_t OllamaClient::healthCheck()
{
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Checking Ollama health");

    // In a real implementation, make a simple GET request to /api/tags or /
    // For simulation, assume success
    return ESP_OK;
}