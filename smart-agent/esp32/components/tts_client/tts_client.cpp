#include "tts_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "TTS_CLIENT";

// WAV header size (standard 44 bytes)
static const size_t WAV_HEADER_SIZE = 44;

// ---------------------------------------------------------------------------
// HTTP response accumulation buffer
// ---------------------------------------------------------------------------

struct TtsHttpBuffer {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
    tts_chunk_callback_t chunkCallback;
    void*                chunkCtx;
};

static bool tts_buffer_append(TtsHttpBuffer* buf, const void* data, size_t len)
{
    if (!data || len == 0) return true;
    if (buf->size + len + 1 > buf->capacity) {
        size_t newCap = buf->capacity + len + 8192;
        uint8_t* newData = (uint8_t*)realloc(buf->data, newCap);
        if (!newData) return false;
        buf->data     = newData;
        buf->capacity = newCap;
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    return true;
}

static esp_err_t tts_http_event_handler(esp_http_client_event_t* evt)
{
    TtsHttpBuffer* buf = (TtsHttpBuffer*)evt->user_data;
    if (!buf) return ESP_OK;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            if (buf->chunkCallback) {
                // Streaming mode: deliver raw bytes to callback, also accumulate
                buf->chunkCallback((const uint8_t*)evt->data, (size_t)evt->data_len, buf->chunkCtx);
            }
            if (!tts_buffer_append(buf, evt->data, evt->data_len)) {
                ESP_LOGE(TAG, "TTS response buffer OOM");
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
// Base-64 decode helper (RFC 4648)
// ---------------------------------------------------------------------------

static const int8_t B64_DECODE_TABLE[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,  // -2 = '='
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

size_t TTSClient::base64Decode(const char* encoded, uint8_t** outBuf)
{
    if (!encoded || !outBuf) return 0;

    size_t encLen  = strlen(encoded);
    size_t maxSize = (encLen / 4) * 3 + 4;
    uint8_t* buf   = (uint8_t*)malloc(maxSize);
    if (!buf) return 0;

    size_t outIdx = 0;
    for (size_t i = 0; i + 3 < encLen; i += 4) {
        int8_t v0 = B64_DECODE_TABLE[(uint8_t)encoded[i]];
        int8_t v1 = B64_DECODE_TABLE[(uint8_t)encoded[i + 1]];
        int8_t v2 = B64_DECODE_TABLE[(uint8_t)encoded[i + 2]];
        int8_t v3 = B64_DECODE_TABLE[(uint8_t)encoded[i + 3]];

        if (v0 < 0 || v1 < 0) break;

        buf[outIdx++] = (uint8_t)((v0 << 2) | (v1 >> 4));
        if (v2 >= 0) buf[outIdx++] = (uint8_t)((v1 << 4) | (v2 >> 2));
        if (v3 >= 0) buf[outIdx++] = (uint8_t)((v2 << 6) | v3);
    }

    *outBuf = buf;
    return outIdx;
}

// ---------------------------------------------------------------------------
// TTSClient — constructor / destructor
// ---------------------------------------------------------------------------

TTSClient::TTSClient()
    : m_provider(TTS_PROVIDER_PIPER)
    , m_voice("en_US-lessac-medium")
    , m_sampleRate(22050)
    , m_timeoutMs(15000)
    , m_maxRetries(3)
    , m_initialized(false)
    , m_audioDriver(nullptr)
{
    m_serverUrl = "http://127.0.0.1:5000";
}

TTSClient::~TTSClient()
{
    deinit();
}

esp_err_t TTSClient::init(AudioDriver* audioDriver)
{
    if (m_initialized) return ESP_OK;
    m_audioDriver = audioDriver;
    m_initialized = true;
    ESP_LOGI(TAG, "TTSClient initialized — provider=%d  url=%s  voice=%s",
             (int)m_provider, m_serverUrl.c_str(), m_voice.c_str());
    return ESP_OK;
}

void TTSClient::deinit()
{
    m_initialized = false;
    m_audioDriver = nullptr;
}

// ---------------------------------------------------------------------------
// Configuration setters
// ---------------------------------------------------------------------------

void TTSClient::setProvider(tts_provider_t p)  { m_provider   = p; }
void TTSClient::setServerUrl(const char* url)  { if (url) m_serverUrl = url; }
void TTSClient::setApiKey(const char* key)     { if (key) m_apiKey    = key; }
void TTSClient::setVoice(const char* voice)    { if (voice) m_voice   = voice; }
void TTSClient::setSampleRate(int sr)          { m_sampleRate = sr > 0 ? sr : 22050; }
void TTSClient::setTimeout(int ms)             { m_timeoutMs  = ms > 0 ? ms : 15000; }
void TTSClient::setMaxRetries(int r)           { m_maxRetries = r >= 0 ? r : 3; }

// ---------------------------------------------------------------------------
// Public speak() overloads
// ---------------------------------------------------------------------------

esp_err_t TTSClient::speak(const char* text, bool blocking)
{
    if (!m_initialized || !text) return ESP_ERR_INVALID_ARG;

    uint8_t* wavBuf = nullptr;
    size_t   wavLen = 0;

    esp_err_t err = ESP_FAIL;
    switch (m_provider) {
    case TTS_PROVIDER_PIPER:
        err = synthesizePiper(text, &wavBuf, wavLen);
        break;
    case TTS_PROVIDER_GOOGLE:
        err = synthesizeGoogle(text, &wavBuf, wavLen);
        break;
    case TTS_PROVIDER_ELEVENLABS:
        err = synthesizeElevenLabs(text, &wavBuf, wavLen);
        break;
    case TTS_PROVIDER_ESPEAK:
        err = synthesizeEspeak(text, &wavBuf, wavLen);
        break;
    default:
        ESP_LOGE(TAG, "Unknown TTS provider");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (err != ESP_OK) {
        freeAudioBuffer(wavBuf);
        return err;
    }

    err = playWavBuffer(wavBuf, wavLen, blocking);
    freeAudioBuffer(wavBuf);
    return err;
}

esp_err_t TTSClient::speak(const std::string& text, bool blocking)
{
    return speak(text.c_str(), blocking);
}

// ---------------------------------------------------------------------------
// speakStream() — deliver each HTTP chunk to callback, then play
// ---------------------------------------------------------------------------

esp_err_t TTSClient::speakStream(const char*          text,
                                  tts_chunk_callback_t callback,
                                  void*                callbackCtx)
{
    if (!m_initialized || !text) return ESP_ERR_INVALID_ARG;

    // Only Piper and eSpeak support streaming with our current back-end setup.
    // For Google/ElevenLabs, fall back to regular synthesis then play.
    if (m_provider == TTS_PROVIDER_GOOGLE || m_provider == TTS_PROVIDER_ELEVENLABS) {
        uint8_t* wavBuf = nullptr;
        size_t   wavLen = 0;
        esp_err_t err = (m_provider == TTS_PROVIDER_GOOGLE)
                        ? synthesizeGoogle(text, &wavBuf, wavLen)
                        : synthesizeElevenLabs(text, &wavBuf, wavLen);
        if (err == ESP_OK) {
            if (callback && wavBuf) {
                callback(wavBuf, wavLen, callbackCtx);
            }
            err = playWavBuffer(wavBuf, wavLen, /*blocking=*/true);
        }
        freeAudioBuffer(wavBuf);
        return err;
    }

    // Piper / eSpeak streaming via HTTP chunk callback
    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/api/tts";

    cJSON* root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "text", text);
    cJSON_AddStringToObject(root, "voice", m_voice.c_str());
    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    TtsHttpBuffer buf = {};
    buf.capacity      = 16384;
    buf.data          = (uint8_t*)malloc(buf.capacity);
    if (!buf.data) { free(payload); return ESP_ERR_NO_MEM; }
    buf.chunkCallback = callback;
    buf.chunkCtx      = callbackCtx;

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = tts_http_event_handler;
    cfg.user_data     = &buf;
    cfg.buffer_size   = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = ESP_FAIL;
    if (client) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload, (int)strlen(payload));
        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (err == ESP_OK && status != 200) err = ESP_FAIL;
    }

    free(payload);

    if (err == ESP_OK && buf.data && buf.size > WAV_HEADER_SIZE) {
        err = playWavBuffer(buf.data, buf.size, /*blocking=*/true);
    }

    free(buf.data);
    return err;
}

// ---------------------------------------------------------------------------
// synthesizePiper — POST <serverUrl>/api/tts
// Body: { "text": "...", "voice": "..." }
// Response: audio/wav
// ---------------------------------------------------------------------------

esp_err_t TTSClient::synthesizePiper(const char* text, uint8_t** wavBuf, size_t& wavLen)
{
    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/api/tts";

    cJSON* root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "text",  text);
    cJSON_AddStringToObject(root, "voice", m_voice.c_str());
    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    TtsHttpBuffer buf = {};
    buf.capacity      = 65536;
    buf.data          = (uint8_t*)malloc(buf.capacity);
    if (!buf.data) { free(payload); return ESP_ERR_NO_MEM; }

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = tts_http_event_handler;
    cfg.user_data     = &buf;
    cfg.buffer_size   = 4096;

    esp_err_t err     = ESP_FAIL;
    int       delayMs = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Piper retry %d/%d", attempt, m_maxRetries);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2;
            buf.size = 0;
        }

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { err = ESP_FAIL; continue; }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload, (int)strlen(payload));

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "Piper TTS failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) { err = ESP_FAIL; break; }
        if (err == ESP_OK) err = ESP_FAIL;
    }

    free(payload);

    if (err == ESP_OK) {
        if (buf.size == 0) {
            ESP_LOGE(TAG, "Piper TTS: empty audio response");
            err = ESP_FAIL;
        } else {
            *wavBuf = buf.data;
            wavLen  = buf.size;
            return ESP_OK; // caller owns buf.data
        }
    }

    free(buf.data);
    return err;
}

// ---------------------------------------------------------------------------
// synthesizeGoogle — POST https://texttospeech.googleapis.com/v1/text:synthesize
// ---------------------------------------------------------------------------

esp_err_t TTSClient::synthesizeGoogle(const char* text, uint8_t** wavBuf, size_t& wavLen)
{
    if (m_apiKey.empty()) {
        ESP_LOGE(TAG, "Google TTS: API key not set");
        return ESP_ERR_INVALID_ARG;
    }

    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/v1/text:synthesize?key=" + m_apiKey;

    cJSON* root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON* input = cJSON_CreateObject();
    cJSON_AddStringToObject(input, "text", text);
    cJSON_AddItemToObject(root, "input", input);

    cJSON* voice = cJSON_CreateObject();
    cJSON_AddStringToObject(voice, "languageCode", "en-US");
    cJSON_AddStringToObject(voice, "name", m_voice.c_str());
    cJSON_AddItemToObject(root, "voice", voice);

    cJSON* audioCfg = cJSON_CreateObject();
    cJSON_AddStringToObject(audioCfg, "audioEncoding", "LINEAR16");
    cJSON_AddNumberToObject(audioCfg, "sampleRateHertz", m_sampleRate);
    cJSON_AddItemToObject(root, "audioConfig", audioCfg);

    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    TtsHttpBuffer buf = {};
    buf.capacity      = 16384;
    buf.data          = (uint8_t*)malloc(buf.capacity);
    if (!buf.data) { free(payload); return ESP_ERR_NO_MEM; }

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = tts_http_event_handler;
    cfg.user_data     = &buf;
    cfg.buffer_size   = 4096;

    esp_err_t err     = ESP_FAIL;
    int       delayMs = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Google TTS retry %d/%d", attempt, m_maxRetries);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2;
            buf.size = 0;
        }

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { err = ESP_FAIL; continue; }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload, (int)strlen(payload));

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "Google TTS failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) { err = ESP_FAIL; break; }
        if (err == ESP_OK) err = ESP_FAIL;
    }

    free(payload);

    if (err != ESP_OK) { free(buf.data); return err; }

    // Google returns JSON: { "audioContent": "<base64 encoded LINEAR16 WAV>" }
    cJSON* resp = cJSON_ParseWithLength((const char*)buf.data, buf.size);
    free(buf.data);

    if (!resp) {
        ESP_LOGE(TAG, "Google TTS: failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON* ac = cJSON_GetObjectItemCaseSensitive(resp, "audioContent");
    if (!cJSON_IsString(ac) || !ac->valuestring) {
        ESP_LOGE(TAG, "Google TTS: no audioContent in response");
        cJSON_Delete(resp);
        return ESP_FAIL;
    }

    wavLen = base64Decode(ac->valuestring, wavBuf);
    cJSON_Delete(resp);

    if (wavLen == 0 || !*wavBuf) {
        ESP_LOGE(TAG, "Google TTS: base64 decode produced no data");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Google TTS: decoded %u bytes of audio", (unsigned)wavLen);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// synthesizeElevenLabs
// POST https://api.elevenlabs.io/v1/text-to-speech/<voice_id>
// ---------------------------------------------------------------------------

esp_err_t TTSClient::synthesizeElevenLabs(const char* text, uint8_t** wavBuf, size_t& wavLen)
{
    if (m_apiKey.empty()) {
        ESP_LOGE(TAG, "ElevenLabs: API key not set");
        return ESP_ERR_INVALID_ARG;
    }

    std::string url = m_serverUrl;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/v1/text-to-speech/" + m_voice;

    cJSON* root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "text", text);
    cJSON* vs = cJSON_CreateObject();
    cJSON_AddStringToObject(vs, "stability", "0.5");
    cJSON_AddStringToObject(vs, "similarity_boost", "0.5");
    cJSON_AddItemToObject(root, "voice_settings", vs);
    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    TtsHttpBuffer buf = {};
    buf.capacity      = 131072;
    buf.data          = (uint8_t*)malloc(buf.capacity);
    if (!buf.data) { free(payload); return ESP_ERR_NO_MEM; }

    std::string authHeader = "xi-api-key: " + m_apiKey;

    esp_http_client_config_t cfg = {};
    cfg.url           = url.c_str();
    cfg.method        = HTTP_METHOD_POST;
    cfg.timeout_ms    = m_timeoutMs;
    cfg.event_handler = tts_http_event_handler;
    cfg.user_data     = &buf;
    cfg.buffer_size   = 4096;

    esp_err_t err     = ESP_FAIL;
    int       delayMs = 500;

    for (int attempt = 0; attempt <= m_maxRetries; ++attempt) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "ElevenLabs retry %d/%d", attempt, m_maxRetries);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
            delayMs *= 2;
            buf.size = 0;
        }

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { err = ESP_FAIL; continue; }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "xi-api-key",   m_apiKey.c_str());
        esp_http_client_set_header(client, "Accept",        "audio/mpeg");
        esp_http_client_set_post_field(client, payload, (int)strlen(payload));

        err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "ElevenLabs failed: err=%s  HTTP=%d", esp_err_to_name(err), status);
        if (status >= 400 && status < 500) { err = ESP_FAIL; break; }
        if (err == ESP_OK) err = ESP_FAIL;
    }

    free(payload);

    if (err == ESP_OK) {
        if (buf.size == 0) { free(buf.data); return ESP_FAIL; }
        *wavBuf = buf.data;
        wavLen  = buf.size;
        ESP_LOGI(TAG, "ElevenLabs: received %u bytes of audio", (unsigned)wavLen);
        return ESP_OK;
    }

    free(buf.data);
    return err;
}

// ---------------------------------------------------------------------------
// synthesizeEspeak — POST <serverUrl>/api/tts
// Expects the same Piper-compatible endpoint but typically on a different port.
// ---------------------------------------------------------------------------

esp_err_t TTSClient::synthesizeEspeak(const char* text, uint8_t** wavBuf, size_t& wavLen)
{
    // eSpeak HTTP server typically uses the same REST API as Piper:
    //   POST /api/tts  { "text": "...", "voice": "..." }
    // Delegate to synthesizePiper which handles this format.
    return synthesizePiper(text, wavBuf, wavLen);
}

// ---------------------------------------------------------------------------
// freeAudioBuffer
// ---------------------------------------------------------------------------

void TTSClient::freeAudioBuffer(uint8_t* buf)
{
    free(buf);
}

// ---------------------------------------------------------------------------
// playWavBuffer — skip WAV header and send raw PCM to AudioDriver
// ---------------------------------------------------------------------------

esp_err_t TTSClient::playWavBuffer(const uint8_t* wavBuf, size_t wavLen, bool blocking)
{
    if (!wavBuf || wavLen == 0) return ESP_ERR_INVALID_ARG;

    if (!m_audioDriver) {
        ESP_LOGW(TAG, "No AudioDriver configured — audio will not be played");
        return ESP_OK;
    }

    // Skip the 44-byte WAV header to get to raw PCM samples
    if (wavLen <= WAV_HEADER_SIZE) {
        ESP_LOGE(TAG, "WAV buffer too small (%u bytes) to contain header", (unsigned)wavLen);
        return ESP_FAIL;
    }

    const uint8_t* pcm    = wavBuf + WAV_HEADER_SIZE;
    size_t         pcmLen = wavLen - WAV_HEADER_SIZE;

    ESP_LOGI(TAG, "Playing %u PCM bytes (blocking=%d)", (unsigned)pcmLen, (int)blocking);

    esp_err_t err = m_audioDriver->play(pcm, pcmLen, blocking);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AudioDriver::play failed: %s", esp_err_to_name(err));
    }
    return err;
}
