#include "stt_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "STT_CLIENT";

// ---------------------------------------------------------------------------
// HTTP response accumulation buffer
// ---------------------------------------------------------------------------

struct HttpBuffer {
    char*  data;
    size_t size;
    size_t capacity;
};

static bool http_buffer_append(HttpBuffer* buf, const void* data, size_t len)
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

static esp_err_t stt_http_event_handler(esp_http_client_event_t* evt)
{
    HttpBuffer* buf = (HttpBuffer*)evt->user_data;
    if (!buf) return ESP_OK;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            if (!http_buffer_append(buf, evt->data, evt->data_len)) {
                ESP_LOGE(TAG, "Response buffer OOM");
            }
        }
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    default:
        break;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Base-64 encoding (RFC 4648, no line wrapping)
// ---------------------------------------------------------------------------

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string STTClient::base64Encode(const uint8_t* data, size_t len)
{
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple  = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) triple |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) triple |= ((uint32_t)data[i + 2]);

        result += B64_TABLE[(triple >> 18) & 0x3F];
        result += B64_TABLE[(triple >> 12) & 0x3F];
        result += (i + 1 < len) ? B64_TABLE[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? B64_TABLE[(triple     ) & 0x3F] : '=';
    }
    return result;
}

// ---------------------------------------------------------------------------
// WAV header builder (PCM, mono, 16-bit, little-endian)
// ---------------------------------------------------------------------------

size_t STTClient::buildWavBuffer(const int16_t* pcmData,
                                  size_t         numSamples,
                                  int            sampleRate,
                                  uint8_t**      outBuf)
{
    const uint16_t NUM_CHANNELS   = 1;
    const uint16_t BITS_PER_SAMPLE = 16;
    const uint32_t byte_rate = (uint32_t)sampleRate * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    const uint32_t data_size  = (uint32_t)(numSamples * sizeof(int16_t));
    const uint32_t total_size = 44 + data_size;

    uint8_t* buf = (uint8_t*)malloc(total_size);
    if (!buf) return 0;

    // RIFF header
    memcpy(buf +  0, "RIFF", 4);
    uint32_t riff_size = 36 + data_size;
    memcpy(buf +  4, &riff_size, 4);
    memcpy(buf +  8, "WAVE", 4);
    // fmt sub-chunk
    memcpy(buf + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(buf + 16, &fmt_size, 4);
    uint16_t audio_format = 1; // PCM
    memcpy(buf + 20, &audio_format, 2);
    memcpy(buf + 22, &NUM_CHANNELS, 2);
    uint32_t sr = (uint32_t)sampleRate;
    memcpy(buf + 24, &sr, 4);
    memcpy(buf + 28, &byte_rate, 4);
    uint16_t block_align = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    memcpy(buf + 32, &block_align, 2);
    memcpy(buf + 34, &BITS_PER_SAMPLE, 2);
    // data sub-chunk
    memcpy(buf + 36, "data", 4);
    memcpy(buf + 40, &data_size, 4);
    memcpy(buf + 44, pcmData, data_size);

    *outBuf = buf;
    return total_size;
}

void STTClient::freeWavBuffer(uint8_t* buf)
{
    free(buf);
}

// ---------------------------------------------------------------------------
// STTClient — constructor / destructor
// ---------------------------------------------------------------------------

STTClient::STTClient()
    : m_provider(STT_PROVIDER_WHISPER)
    , m_language("en")
    , m_timeoutMs(10000)
    , m_maxRetries(3)
    , m_initialized(false)
{
    m_serverUrl = "http://127.0.0.1:9000";
}

STTClient::~STTClient()
{
    deinit();
}

esp_err_t STTClient::init()
{
    if (m_initialized) return ESP_OK;
    m_initialized = true;
    ESP_LOGI(TAG, "STTClient initialized — provider=%s  url=%s",
             m_provider == STT_PROVIDER_WHISPER ? "whisper" : "google",
             m_serverUrl.c_str());
    return ESP_OK;
}

void STTClient::deinit()
{
    m_initialized = false;
}

// ---------------------------------------------------------------------------
// Configuration setters
// ---------------------------------------------------------------------------

void STTClient::setProvider(stt_provider_t provider) { m_provider   = provider; }
void STTClient::setServerUrl(const char* url)         { if (url) m_serverUrl = url; }
void STTClient::setApiKey(const char* key)            { if (key) m_apiKey    = key; }
void STTClient::setLanguage(const char* lang)         { if (lang) m_language  = lang; }
void STTClient::setTimeout(int ms)                    { m_timeoutMs = ms > 0 ? ms : 10000; }
void STTClient::setMaxRetries(int r)                  { m_maxRetries = r >= 0 ? r : 3; }

// ---------------------------------------------------------------------------
// Public transcribe() — dispatches to provider-specific implementation
// ---------------------------------------------------------------------------

esp_err_t STTClient::transcribe(const int16_t* pcmData,
                                 size_t         numSamples,
                                 int            sampleRate,
                                 std::string&   transcript)
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;
    if (!pcmData || numSamples == 0) return ESP_ERR_INVALID_ARG;

    uint8_t* wavBuf  = nullptr;
    size_t   wavLen  = buildWavBuffer(pcmData, numSamples, sampleRate, &wavBuf);
    if (wavLen == 0 || !wavBuf) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_FAIL;
    switch (m_provider) {
    case STT_PROVIDER_WHISPER:
        err = transcribeWhisper(wavBuf, wavLen, transcript);
        break;
    case STT_PROVIDER_GOOGLE:
        err = transcribeGoogle(pcmData, numSamples, sampleRate, transcript);
        break;
    default:
        ESP_LOGE(TAG, "Unknown STT provider");
        err = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    freeWavBuffer(wavBuf);
    return err;
}

// ---------------------------------------------------------------------------
// Whisper HTTP transcription
// POST <serverUrl>/inference   Content-Type: multipart/form-data
// Response: { "text": "..." }
// ---------------------------------------------------------------------------

esp_err_t STTClient::transcribeWhisper(const uint8_t* wavBuf,
                                        size_t         wavLen,
                                        std::string&   transcript)
{
    // Build the URL
    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/inference";

    // Build multipart/form-data body.
    // Boundary: "----STTBoundary"
    const char* boundary = "----STTBoundary";
    std::string contentType = std::string("multipart/form-data; boundary=") + boundary;

    // Part header
    std::string partHeader =
        std::string("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";

    std::string partFooter = std::string("\r\n--") + boundary + "--\r\n";

    size_t bodyLen = partHeader.size() + wavLen + partFooter.size();
    uint8_t* body  = (uint8_t*)malloc(bodyLen);
    if (!body) return ESP_ERR_NO_MEM;

    size_t offset = 0;
    memcpy(body + offset, partHeader.c_str(), partHeader.size()); offset += partHeader.size();
    memcpy(body + offset, wavBuf, wavLen);                        offset += wavLen;
    memcpy(body + offset, partFooter.c_str(), partFooter.size()); offset += partFooter.size();

    // Allocate response buffer
    HttpBuffer respBuf = {};
    respBuf.capacity   = 4096;
    respBuf.data       = (char*)malloc(respBuf.capacity);
    if (!respBuf.data) { free(body); return ESP_ERR_NO_MEM; }
    respBuf.data[0]    = '\0';

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = stt_http_event_handler;
    cfg.user_data     = &respBuf;
    cfg.buffer_size   = 4096;

    esp_err_t err    = ESP_FAIL;
    int       delayMs = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Whisper retry %d/%d", attempt, m_maxRetries);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2;
            respBuf.size    = 0;
            respBuf.data[0] = '\0';
        }

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { err = ESP_FAIL; continue; }

        esp_http_client_set_header(client, "Content-Type", contentType.c_str());
        esp_http_client_set_post_field(client, (const char*)body, (int)bodyLen);

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "Whisper request failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) { err = ESP_FAIL; break; }
        if (err == ESP_OK) err = ESP_FAIL;
    }

    free(body);

    if (err == ESP_OK) {
        err = parseWhisperResponse(respBuf.data, transcript);
    }

    free(respBuf.data);
    return err;
}

// ---------------------------------------------------------------------------
// Google Cloud Speech-to-Text transcription
// POST https://speech.googleapis.com/v1/speech:recognize?key=<API_KEY>
// Body JSON: { config: { encoding, sampleRateHertz, languageCode },
//              audio:  { content: "<base64-encoded PCM>" } }
// ---------------------------------------------------------------------------

esp_err_t STTClient::transcribeGoogle(const int16_t* pcmData,
                                       size_t         numSamples,
                                       int            sampleRate,
                                       std::string&   transcript)
{
    if (m_apiKey.empty()) {
        ESP_LOGE(TAG, "Google STT: API key not set");
        return ESP_ERR_INVALID_ARG;
    }

    // Build URL
    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/v1/speech:recognize?key=" + m_apiKey;

    // Base-64 encode raw PCM (Google expects LINEAR16 encoded audio)
    std::string audioB64 = base64Encode(
        reinterpret_cast<const uint8_t*>(pcmData),
        numSamples * sizeof(int16_t));

    // Build JSON request
    cJSON* root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON* config = cJSON_CreateObject();
    cJSON_AddStringToObject(config, "encoding", "LINEAR16");
    cJSON_AddNumberToObject(config, "sampleRateHertz", sampleRate);
    cJSON_AddStringToObject(config, "languageCode", m_language.c_str());
    cJSON_AddItemToObject(root, "config", config);

    cJSON* audio = cJSON_CreateObject();
    cJSON_AddStringToObject(audio, "content", audioB64.c_str());
    cJSON_AddItemToObject(root, "audio", audio);

    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    // Send request
    HttpBuffer respBuf = {};
    respBuf.capacity   = 8192;
    respBuf.data       = (char*)malloc(respBuf.capacity);
    if (!respBuf.data) { free(payload); return ESP_ERR_NO_MEM; }
    respBuf.data[0]    = '\0';

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = stt_http_event_handler;
    cfg.user_data     = &respBuf;
    cfg.buffer_size   = 4096;

    esp_err_t err     = ESP_FAIL;
    int       delayMs = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Google STT retry %d/%d", attempt, m_maxRetries);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2;
            respBuf.size    = 0;
            respBuf.data[0] = '\0';
        }

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { err = ESP_FAIL; continue; }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload, (int)strlen(payload));

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "Google STT failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) { err = ESP_FAIL; break; }
        if (err == ESP_OK) err = ESP_FAIL;
    }

    free(payload);

    if (err == ESP_OK) {
        err = parseGoogleResponse(respBuf.data, transcript);
    }

    free(respBuf.data);
    return err;
}

// ---------------------------------------------------------------------------
// JSON response parsers
// ---------------------------------------------------------------------------

esp_err_t STTClient::parseWhisperResponse(const char* body, std::string& transcript)
{
    if (!body || body[0] == '\0') {
        ESP_LOGE(TAG, "Whisper: empty response body");
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGE(TAG, "Whisper: JSON parse failed: %.200s", body);
        return ESP_FAIL;
    }

    // Whisper server returns { "text": "..." }
    cJSON* text = cJSON_GetObjectItemCaseSensitive(root, "text");
    esp_err_t err = ESP_FAIL;

    if (cJSON_IsString(text) && text->valuestring) {
        transcript = text->valuestring;
        // Trim leading/trailing whitespace
        while (!transcript.empty() && (transcript.front() == ' ' || transcript.front() == '\n'))
            transcript.erase(transcript.begin());
        while (!transcript.empty() && (transcript.back() == ' ' || transcript.back() == '\n'))
            transcript.pop_back();

        if (transcript.empty()) {
            ESP_LOGW(TAG, "Whisper: empty transcript");
            err = ESP_ERR_NOT_FOUND;
        } else {
            ESP_LOGI(TAG, "Whisper transcript: %s", transcript.c_str());
            err = ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "Whisper: no 'text' field in response");
    }

    cJSON_Delete(root);
    return err;
}

esp_err_t STTClient::parseGoogleResponse(const char* body, std::string& transcript)
{
    if (!body || body[0] == '\0') {
        ESP_LOGE(TAG, "Google STT: empty response body");
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGE(TAG, "Google STT: JSON parse failed");
        return ESP_FAIL;
    }

    // { results: [{ alternatives: [{ transcript: "..." }] }] }
    esp_err_t err    = ESP_FAIL;
    cJSON*    results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (cJSON_IsArray(results) && cJSON_GetArraySize(results) > 0) {
        cJSON* first = cJSON_GetArrayItem(results, 0);
        cJSON* alts  = cJSON_GetObjectItemCaseSensitive(first, "alternatives");
        if (cJSON_IsArray(alts) && cJSON_GetArraySize(alts) > 0) {
            cJSON* alt   = cJSON_GetArrayItem(alts, 0);
            cJSON* tnode = cJSON_GetObjectItemCaseSensitive(alt, "transcript");
            if (cJSON_IsString(tnode) && tnode->valuestring) {
                transcript = tnode->valuestring;
                ESP_LOGI(TAG, "Google STT transcript: %s", transcript.c_str());
                err = ESP_OK;
            }
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Google STT: no transcript in response: %.200s", body);
    }

    cJSON_Delete(root);
    return err;
}
