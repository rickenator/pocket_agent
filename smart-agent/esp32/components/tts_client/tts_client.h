#ifndef TTS_CLIENT_H
#define TTS_CLIENT_H

#include <stdint.h>
#include <string>
#include "esp_err.h"
#include "audio_driver.h"

// Text-to-Speech provider selection
typedef enum {
    TTS_PROVIDER_PIPER = 0,    // Local Piper TTS HTTP server
    TTS_PROVIDER_ESPEAK,       // Local eSpeak-ng HTTP server
    TTS_PROVIDER_GOOGLE,       // Google Cloud Text-to-Speech REST API
    TTS_PROVIDER_ELEVENLABS,   // ElevenLabs cloud TTS API
} tts_provider_t;

// TTS client — converts text to speech audio by calling a remote TTS service,
// then streams the returned WAV/PCM audio through the local AudioDriver (I2S speaker).
//
// Typical deployment (Piper TTS server on local network):
//   http://192.168.1.251:5000
//   Endpoint: POST /api/tts  Content-Type: application/json
//   Body:    { "text": "Hello world", "voice": "en_US-lessac-medium" }
//   Response: audio/wav  (raw WAV bytes)
//
// Typical deployment (Google Cloud TTS):
//   POST https://texttospeech.googleapis.com/v1/text:synthesize?key=<API_KEY>
//   Body JSON: { input: { text: "..." },
//                voice: { languageCode: "en-US", name: "en-US-Standard-A" },
//                audioConfig: { audioEncoding: "LINEAR16", sampleRateHertz: 22050 } }
//   Response JSON: { audioContent: "<base64-encoded WAV>" }
class TTSClient {
public:
    TTSClient();
    ~TTSClient();

    // Initialise HTTP resources and bind to an AudioDriver instance.
    // audioDriver must remain valid for the lifetime of this TTSClient.
    esp_err_t init(AudioDriver* audioDriver);
    void deinit();

    // ---- Configuration ----

    // Select TTS provider (default: TTS_PROVIDER_PIPER).
    void setProvider(tts_provider_t provider);

    // Base URL of the TTS service, e.g. "http://192.168.1.251:5000".
    // For Google, use "https://texttospeech.googleapis.com".
    void setServerUrl(const char* url);

    // Cloud-only: API key for authentication.
    void setApiKey(const char* apiKey);

    // Voice / model identifier understood by the provider.
    //   Piper:      "en_US-lessac-medium", "en_GB-alan-medium", etc.
    //   Google:     "en-US-Standard-A", "en-US-Wavenet-D", etc.
    //   ElevenLabs: voice UUID string.
    // (default: "en_US-lessac-medium")
    void setVoice(const char* voice);

    // Output sample rate in Hz; must match the AudioDriver configuration.
    // (default: 22 050 Hz for Piper; 16 000 Hz for Google LINEAR16)
    void setSampleRate(int sampleRate);

    // HTTP request timeout in milliseconds (default: 15 000 ms).
    void setTimeout(int timeout_ms);

    // Number of retry attempts on transient network failures (default: 3).
    void setMaxRetries(int retries);

    // ---- Speech synthesis ----

    // Convert text to speech and play through AudioDriver.
    //
    // Parameters:
    //   text      — null-terminated UTF-8 text to synthesize
    //   blocking  — if true, blocks until audio playback completes
    //
    // Returns:
    //   ESP_OK          — synthesis and playback started (or completed if blocking)
    //   ESP_ERR_TIMEOUT — TTS server did not respond within timeout
    //   ESP_FAIL        — HTTP error, empty audio response, or playback failure
    esp_err_t speak(const char* text, bool blocking = true);

    // Convenience: speak from a std::string.
    esp_err_t speak(const std::string& text, bool blocking = true);

    // Stream synthesis: fetch audio and invoke callback for each PCM chunk
    // before writing to the AudioDriver.  Useful for progress indicators.
    typedef void (*tts_chunk_callback_t)(const uint8_t* chunk, size_t len, void* ctx);
    esp_err_t speakStream(const char* text,
                          tts_chunk_callback_t callback = nullptr,
                          void* callbackCtx = nullptr);

private:
    // Provider-specific synthesis implementations — return raw WAV bytes.
    esp_err_t synthesizePiper(const char* text, uint8_t** wavBuf, size_t& wavLen);
    esp_err_t synthesizeGoogle(const char* text, uint8_t** wavBuf, size_t& wavLen);
    esp_err_t synthesizeElevenLabs(const char* text, uint8_t** wavBuf, size_t& wavLen);
    esp_err_t synthesizeEspeak(const char* text, uint8_t** wavBuf, size_t& wavLen);

    void freeAudioBuffer(uint8_t* buf);

    // Skip the WAV header (44 bytes) and feed raw PCM to AudioDriver.
    esp_err_t playWavBuffer(const uint8_t* wavBuf, size_t wavLen, bool blocking);

    // Base-64 decode helper (used to decode Google TTS audioContent).
    size_t base64Decode(const char* encoded, uint8_t** outBuf);

    tts_provider_t m_provider;
    std::string    m_serverUrl;
    std::string    m_apiKey;
    std::string    m_voice;
    int            m_sampleRate;
    int            m_timeoutMs;
    int            m_maxRetries;
    bool           m_initialized;
    AudioDriver*   m_audioDriver;
};

#endif // TTS_CLIENT_H
