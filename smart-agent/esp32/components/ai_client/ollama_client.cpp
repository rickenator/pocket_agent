#include "ollama_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "OLLAMA_CLIENT";

// ---------------------------------------------------------------------------
// Response buffer helpers
// ---------------------------------------------------------------------------

static bool buffer_append(OllamaResponseBuffer* buf, const void* data, size_t len)
{
    if (!data || len == 0) return true;
    if (buf->size + len + 1 > buf->capacity) {
        size_t newCap = buf->capacity + len + 4096;
        char* newData = (char*)realloc(buf->data, newCap);
        if (!newData) return false;
        buf->data     = newData;
        buf->capacity = newCap;
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    return true;
}

// Accumulate into the fullContent buffer (used by streaming to collect assistant reply)
static void buffer_append_full(OllamaResponseBuffer* buf, const char* data, size_t len)
{
    if (!buf || !data || len == 0) return;
    if (buf->fullContentSize + len + 1 > buf->fullContentCapacity) {
        size_t newCap = buf->fullContentCapacity + len + 2048;
        char* newData = (char*)realloc(buf->fullContent, newCap);
        if (!newData) return;
        buf->fullContent         = newData;
        buf->fullContentCapacity = newCap;
    }
    memcpy(buf->fullContent + buf->fullContentSize, data, len);
    buf->fullContentSize += len;
    buf->fullContent[buf->fullContentSize] = '\0';
}

// ---------------------------------------------------------------------------
// HTTP event handler
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::httpEventHandler(esp_http_client_event_t* evt)
{
    OllamaResponseBuffer* buf = (OllamaResponseBuffer*)evt->user_data;
    if (!buf) return ESP_OK;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            if (buf->streamCallback) {
                // Streaming mode: accumulate partial data into the buffer and
                // scan for complete newline-delimited JSON lines.
                if (!buffer_append(buf, evt->data, evt->data_len)) {
                    ESP_LOGE(TAG, "Stream buffer OOM");
                    break;
                }
                // Parse complete lines
                char* line_start = buf->data;
                char* newline;
                while ((newline = strchr(line_start, '\n')) != nullptr) {
                    *newline = '\0';
                    if (*line_start != '\0') {
                        cJSON* obj = cJSON_Parse(line_start);
                        if (obj) {
                            cJSON* msg  = cJSON_GetObjectItemCaseSensitive(obj, "message");
                            cJSON* done = cJSON_GetObjectItemCaseSensitive(obj, "done");
                            if (msg) {
                                cJSON* content = cJSON_GetObjectItemCaseSensitive(msg, "content");
                                if (cJSON_IsString(content) && content->valuestring &&
                                    content->valuestring[0] != '\0') {
                                    buf->streamCallback(content->valuestring);
                                    // Accumulate for history tracking
                                    buffer_append_full(buf, content->valuestring,
                                                       strlen(content->valuestring));
                                }
                            }
                            if (cJSON_IsTrue(done)) {
                                // Signal completion with empty string
                                buf->streamCallback("");
                            }
                            cJSON_Delete(obj);
                        }
                    }
                    line_start = newline + 1;
                }
                // Move remaining partial line to beginning of buffer
                size_t remaining = buf->size - (size_t)(line_start - buf->data);
                if (remaining > 0 && line_start != buf->data) {
                    memmove(buf->data, line_start, remaining);
                }
                buf->size = remaining;
                if (buf->data) buf->data[buf->size] = '\0';
            } else {
                // Non-streaming mode: just accumulate all data
                if (!buffer_append(buf, evt->data, evt->data_len)) {
                    ESP_LOGE(TAG, "Response buffer OOM");
                }
            }
        }
        break;

    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_FINISH:
    case HTTP_EVENT_DISCONNECTED:
    default:
        break;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// OllamaClient
// ---------------------------------------------------------------------------

OllamaClient::OllamaClient()
    : m_streamCallback(nullptr)
    , m_mutex(nullptr)
    , m_initialized(false)
    , m_timeoutMs(DEFAULT_TIMEOUT_MS)
    , m_maxRetries(DEFAULT_MAX_RETRIES)
{
    m_serverUrl = "http://127.0.0.1:11434";
    m_model     = "llama2";
}

OllamaClient::~OllamaClient()
{
    deinit();
}

esp_err_t OllamaClient::init()
{
    if (m_initialized) return ESP_OK;

    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "OllamaClient initialized — server: %s  model: %s",
             m_serverUrl.c_str(), m_model.c_str());
    return ESP_OK;
}

void OllamaClient::deinit()
{
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    m_initialized = false;
}

void OllamaClient::setServerUrl(const char* url)
{
    if (url) {
        m_serverUrl = url;
        ESP_LOGI(TAG, "Server URL: %s", url);
    }
}

void OllamaClient::setModel(const char* model)
{
    if (model) {
        m_model = model;
        ESP_LOGI(TAG, "Model: %s", model);
    }
}

void OllamaClient::setStreamCallback(ollama_stream_callback_t callback)
{
    m_streamCallback = callback;
}

void OllamaClient::setTimeout(int timeout_ms)
{
    m_timeoutMs = timeout_ms > 0 ? timeout_ms : DEFAULT_TIMEOUT_MS;
}

void OllamaClient::setMaxRetries(int retries)
{
    m_maxRetries = retries >= 0 ? retries : DEFAULT_MAX_RETRIES;
}

void OllamaClient::addSystemPrompt(const char* prompt)
{
    if (!prompt) return;
    // Remove any existing system message first
    for (auto it = m_conversationHistory.begin(); it != m_conversationHistory.end(); ) {
        if (it->role == "system") it = m_conversationHistory.erase(it);
        else ++it;
    }
    m_conversationHistory.insert(m_conversationHistory.begin(), {"system", prompt});
}

void OllamaClient::addToHistory(const char* role, const char* content)
{
    if (role && content) {
        m_conversationHistory.push_back({role, content});
    }
}

void OllamaClient::clearHistory()
{
    m_conversationHistory.clear();
}

const std::vector<OllamaMessage>& OllamaClient::getHistory() const
{
    return m_conversationHistory;
}

std::string OllamaClient::buildUrl(const char* endpoint) const
{
    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    if (endpoint && endpoint[0] != '/') url += '/';
    if (endpoint) url += endpoint;
    return url;
}

std::string OllamaClient::buildMessagesJson(const char* userPrompt) const
{
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return "[]";

    // Add conversation history
    for (const auto& msg : m_conversationHistory) {
        cJSON* item = cJSON_CreateObject();
        if (!item) continue;
        cJSON_AddStringToObject(item, "role", msg.role.c_str());
        cJSON_AddStringToObject(item, "content", msg.content.c_str());
        cJSON_AddItemToArray(arr, item);
    }

    // Add new user prompt
    if (userPrompt) {
        cJSON* item = cJSON_CreateObject();
        if (item) {
            cJSON_AddStringToObject(item, "role", "user");
            cJSON_AddStringToObject(item, "content", userPrompt);
            cJSON_AddItemToArray(arr, item);
        }
    }

    char* json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return "[]";
    std::string result(json);
    free(json);
    return result;
}

// ---------------------------------------------------------------------------
// sendRequest — non-streaming HTTP POST (or GET when isGet=true)
// Retries up to m_maxRetries times with exponential back-off.
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::sendRequest(const char* endpoint, const char* payload,
                                     std::string& response, bool isGet)
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    std::string url = buildUrl(endpoint);
    ESP_LOGI(TAG, "%s %s  payload_len=%d",
             isGet ? "GET" : "POST", url.c_str(),
             payload ? (int)strlen(payload) : 0);

    // Allocate response buffer
    OllamaResponseBuffer buf = {};
    buf.capacity = 8192;
    buf.data     = (char*)malloc(buf.capacity);
    if (!buf.data) return ESP_ERR_NO_MEM;
    buf.data[0]  = '\0';

    esp_http_client_config_t config = {};
    config.url           = url.c_str();
    config.method        = isGet ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    config.timeout_ms    = m_timeoutMs;
    config.event_handler = OllamaClient::httpEventHandler;
    config.user_data     = &buf;
    config.buffer_size   = 4096;

    esp_err_t err = ESP_FAIL;
    int delayMs   = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Retry %d/%d after %dms", attempt, m_maxRetries, delayMs);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2; // exponential back-off
            // Reset buffer for retry
            buf.size    = 0;
            buf.data[0] = '\0';
        }

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "esp_http_client_init failed");
            err = ESP_FAIL;
            continue;
        }

        if (!isGet && payload) {
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, payload, strlen(payload));
        }

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) {
            break; // success
        }

        ESP_LOGW(TAG, "Request failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) {
            // Client-side error — no point retrying
            err = ESP_FAIL;
            break;
        }
        if (err == ESP_OK && status != 200) {
            err = ESP_FAIL;
        }
    }

    if (err == ESP_OK) {
        response = buf.data ? buf.data : "";
    }
    free(buf.data);
    return err;
}

// ---------------------------------------------------------------------------
// sendRequestStreaming — POST with NDJSON streaming
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::sendRequestStreaming(const char* endpoint, const char* payload,
                                              ollama_stream_callback_t callback)
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    std::string url = buildUrl(endpoint);
    ESP_LOGI(TAG, "STREAM POST %s", url.c_str());

    OllamaResponseBuffer buf = {};
    buf.capacity             = 4096;
    buf.data                 = (char*)malloc(buf.capacity);
    if (!buf.data) return ESP_ERR_NO_MEM;
    buf.data[0]              = '\0';
    buf.streamCallback       = callback;
    // Accumulator for collecting the full assistant response (used to update history)
    buf.fullContentCapacity  = 2048;
    buf.fullContent          = (char*)malloc(buf.fullContentCapacity);
    if (!buf.fullContent) {
        free(buf.data);
        return ESP_ERR_NO_MEM;
    }
    buf.fullContent[0] = '\0';

    esp_http_client_config_t config = {};
    config.url           = url.c_str();
    config.method        = HTTP_METHOD_POST;
    config.timeout_ms    = m_timeoutMs;
    config.event_handler = OllamaClient::httpEventHandler;
    config.user_data     = &buf;
    config.buffer_size   = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(buf.data);
        free(buf.fullContent);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (payload) {
        esp_http_client_set_post_field(client, payload, strlen(payload));
    }

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    // Add the accumulated assistant response to history before freeing
    if (err == ESP_OK && status == 200 && buf.fullContent && buf.fullContent[0] != '\0') {
        m_conversationHistory.push_back({"assistant", buf.fullContent});
    }

    free(buf.data);
    free(buf.fullContent);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Streaming request failed: %s", esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "Streaming request HTTP %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// createChatCompletion (non-streaming)
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::createChatCompletion(const char* prompt, std::string& response)
{
    if (!m_initialized || !prompt) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    // Build JSON payload with cJSON (handles special characters safely)
    cJSON* root = cJSON_CreateObject();
    if (!root) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }

    cJSON_AddStringToObject(root, "model", m_model.c_str());
    cJSON_AddFalseToObject(root, "stream");

    // Build messages array from history + new user prompt
    std::string messagesJson = buildMessagesJson(prompt);
    cJSON* messages = cJSON_Parse(messagesJson.c_str());
    if (!messages) {
        cJSON_Delete(root);
        xSemaphoreGive(m_mutex);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "messages", messages);

    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!payload) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }

    std::string rawResponse;
    esp_err_t err = sendRequest("/api/chat", payload, rawResponse);
    free(payload);

    if (err != ESP_OK) {
        xSemaphoreGive(m_mutex);
        return err;
    }

    // Parse the response JSON to extract message.content
    cJSON* resp = cJSON_Parse(rawResponse.c_str());
    if (!resp) {
        ESP_LOGE(TAG, "Failed to parse chat response JSON");
        xSemaphoreGive(m_mutex);
        return ESP_FAIL;
    }

    cJSON* msg     = cJSON_GetObjectItemCaseSensitive(resp, "message");
    cJSON* content = msg ? cJSON_GetObjectItemCaseSensitive(msg, "content") : nullptr;

    if (cJSON_IsString(content) && content->valuestring) {
        response = content->valuestring;
        // Add assistant reply to conversation history
        m_conversationHistory.push_back({"user", prompt});
        m_conversationHistory.push_back({"assistant", response});
        ESP_LOGI(TAG, "Chat response (%u chars)", (unsigned)response.size());
    } else {
        ESP_LOGE(TAG, "No message.content in response");
        err = ESP_FAIL;
    }

    cJSON_Delete(resp);
    xSemaphoreGive(m_mutex);
    return err;
}

// ---------------------------------------------------------------------------
// createChatCompletionStream
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::createChatCompletionStream(const char* prompt,
                                                    ollama_stream_callback_t callback)
{
    if (!m_initialized || !prompt || !callback) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    cJSON* root = cJSON_CreateObject();
    if (!root) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }

    cJSON_AddStringToObject(root, "model", m_model.c_str());
    cJSON_AddTrueToObject(root, "stream");

    std::string messagesJson = buildMessagesJson(prompt);
    cJSON* messages = cJSON_Parse(messagesJson.c_str());
    if (!messages) {
        cJSON_Delete(root);
        xSemaphoreGive(m_mutex);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "messages", messages);

    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }

    // Add user prompt to history before sending
    m_conversationHistory.push_back({"user", prompt});

    esp_err_t err = sendRequestStreaming("/api/chat", payload, callback);
    free(payload);

    xSemaphoreGive(m_mutex);
    return err;
}

// ---------------------------------------------------------------------------
// listModels — GET /api/tags
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::listModels(std::vector<std::string>& models)
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    std::string response;
    esp_err_t err = sendRequest("/api/tags", nullptr, response, /*isGet=*/true);

    xSemaphoreGive(m_mutex);

    if (err != ESP_OK) return err;

    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse /api/tags response");
        return ESP_FAIL;
    }

    cJSON* modelsArr = cJSON_GetObjectItemCaseSensitive(root, "models");
    if (cJSON_IsArray(modelsArr)) {
        cJSON* m = nullptr;
        cJSON_ArrayForEach(m, modelsArr) {
            cJSON* name = cJSON_GetObjectItemCaseSensitive(m, "name");
            if (cJSON_IsString(name) && name->valuestring) {
                models.push_back(name->valuestring);
                ESP_LOGI(TAG, "Model: %s", name->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// showModel — POST /api/show
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::showModel(const char* model, std::string& info)
{
    if (!m_initialized || !model) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    cJSON* root = cJSON_CreateObject();
    if (!root) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }
    cJSON_AddStringToObject(root, "name", model);
    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) { xSemaphoreGive(m_mutex); return ESP_ERR_NO_MEM; }

    esp_err_t err = sendRequest("/api/show", payload, info);
    free(payload);

    xSemaphoreGive(m_mutex);
    return err;
}

// ---------------------------------------------------------------------------
// healthCheck — GET /api/tags, expect HTTP 200
// ---------------------------------------------------------------------------

esp_err_t OllamaClient::healthCheck()
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    std::string response;
    esp_err_t err = sendRequest("/api/tags", nullptr, response, /*isGet=*/true);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Ollama health check OK");
    } else {
        ESP_LOGW(TAG, "Ollama health check FAILED: %s", esp_err_to_name(err));
    }
    return err;
}
