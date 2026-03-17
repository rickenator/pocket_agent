#ifndef STT_CLIENT_H
#define STT_CLIENT_H

#include <stdint.h>
#include <string>
#include "esp_err.h"

// Speech-to-Text provider selection
typedef enum {
    STT_PROVIDER_WHISPER = 0,  // Local Whisper HTTP server (e.g. faster-whisper-server)
    STT_PROVIDER_GOOGLE,       // Google Cloud Speech-to-Text REST API
} stt_provider_t;

// STT client — sends raw PCM audio to an external STT service and returns a transcript.
//
// Typical deployment (Whisper):
//   A faster-whisper-server or whisper.cpp HTTP server on the local network:
//     http://192.168.1.251:9000
//   Endpoint: POST /inference  (multipart/form-data, field "file": audio.wav)
//   Response JSON: { "text": "transcribed text" }
//
// Typical deployment (Google):
//   POST https://speech.googleapis.com/v1/speech:recognize?key=<API_KEY>
//   Body JSON: { config: { encoding, sampleRateHertz, languageCode },
//                audio:  { content: "<base64-encoded WAV>" } }
//   Response JSON: { results: [{ alternatives: [{ transcript: "..." }] }] }
class STTClient {
public:
    STTClient();
    ~STTClient();

    // Initialise HTTP resources (must be called before transcribe()).
    esp_err_t init();
    void deinit();

    // ---- Configuration ----

    // Select provider (default: STT_PROVIDER_WHISPER).
    void setProvider(stt_provider_t provider);

    // Base URL of the STT service, e.g. "http://192.168.1.251:9000".
    // For Google, use "https://speech.googleapis.com".
    void setServerUrl(const char* url);

    // Google-only: API key for authentication.
    void setApiKey(const char* apiKey);

    // BCP-47 language tag, e.g. "en-US" (default: "en").
    void setLanguage(const char* language);

    // HTTP request timeout in milliseconds (default: 10 000 ms).
    void setTimeout(int timeout_ms);

    // Number of retry attempts on transient network failures (default: 3).
    void setMaxRetries(int retries);

    // ---- Transcription ----

    // Transcribe raw 16-bit PCM audio captured at the given sample rate.
    //
    // Parameters:
    //   pcmData    — pointer to signed 16-bit PCM samples (mono, little-endian)
    //   numSamples — total number of samples in the buffer
    //   sampleRate — capture sample rate in Hz (typically 16 000)
    //   transcript — [out] transcribed text string on ESP_OK
    //
    // Returns:
    //   ESP_OK            — transcription succeeded; transcript is populated
    //   ESP_ERR_TIMEOUT   — server did not respond within timeout
    //   ESP_ERR_NOT_FOUND — server returned an empty transcript
    //   ESP_FAIL          — HTTP error or JSON parse failure
    esp_err_t transcribe(const int16_t* pcmData,
                         size_t         numSamples,
                         int            sampleRate,
                         std::string&   transcript);

private:
    // Build a minimal WAV header + PCM body into a heap buffer.
    // Caller must free with freeWavBuffer().  Returns total bytes written.
    size_t buildWavBuffer(const int16_t* pcmData,
                          size_t         numSamples,
                          int            sampleRate,
                          uint8_t**      outBuf);

    void freeWavBuffer(uint8_t* buf);

    // Provider-specific HTTP request implementations.
    esp_err_t transcribeWhisper(const uint8_t* wavBuf,
                                size_t         wavLen,
                                std::string&   transcript);

    esp_err_t transcribeGoogle(const int16_t* pcmData,
                               size_t         numSamples,
                               int            sampleRate,
                               std::string&   transcript);

    // JSON response parsers.
    esp_err_t parseWhisperResponse(const char* body, std::string& transcript);
    esp_err_t parseGoogleResponse(const char* body, std::string& transcript);

    // Base-64 encode binary data (used for Google STT audio.content field).
    std::string base64Encode(const uint8_t* data, size_t len);

    stt_provider_t m_provider;
    std::string    m_serverUrl;
    std::string    m_apiKey;
    std::string    m_language;
    int            m_timeoutMs;
    int            m_maxRetries;
    bool           m_initialized;
};

#endif // STT_CLIENT_H
