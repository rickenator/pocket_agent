#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <stdint.h>
#include <string.h>
#include "esp_err.h"

// Audio sample types
typedef enum {
    AUDIO_SAMPLE_RATE_8000 = 8000,
    AUDIO_SAMPLE_RATE_16000 = 16000,
    AUDIO_SAMPLE_RATE_44100 = 44100,
} audio_sample_rate_t;

// Audio driver class
class AudioDriver {
public:
    AudioDriver();
    ~AudioDriver();

    esp_err_t init(audio_sample_rate_t sample_rate);
    void deinit();

    // Audio input
    esp_err_t startMic();
    void stopMic();

    // Audio output
    esp_err_t startSpeaker();
    void stopSpeaker();

    // Get audio data
    int readAudioData(uint8_t* buffer, int length);
    void writeAudioData(const uint8_t* buffer, int length);

private:
    bool m_micEnabled;
    bool m_speakerEnabled;
    audio_sample_rate_t m_sampleRate;
    uint8_t m_inputBuffer[1024];
    uint8_t m_outputBuffer[1024];
};

#endif // AUDIO_DRIVER_H