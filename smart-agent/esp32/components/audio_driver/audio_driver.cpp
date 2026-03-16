#include "audio_driver.h"
#include <stdio.h>
#include <string.h>

AudioDriver::AudioDriver()
    : m_micEnabled(false)
    , m_speakerEnabled(false)
    , m_sampleRate(AUDIO_SAMPLE_RATE_16000)
{
    memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
}

AudioDriver::~AudioDriver()
{
    deinit();
}

esp_err_t AudioDriver::init(audio_sample_rate_t sample_rate)
{
    m_sampleRate = sample_rate;
    m_micEnabled = false;
    m_speakerEnabled = false;
    printf("[AUDIO] Audio driver initialized with %d Hz\n", sample_rate);
    return ESP_OK;
}

void AudioDriver::deinit()
{
    if (m_micEnabled) {
        stopMic();
    }
    if (m_speakerEnabled) {
        stopSpeaker();
    }
    printf("[AUDIO] Audio driver deinitialized\n");
}

esp_err_t AudioDriver::startMic()
{
    if (m_micEnabled) {
        return ESP_OK;
    }

    m_micEnabled = true;
    printf("[AUDIO] Microphone started\n");
    return ESP_OK;
}

void AudioDriver::stopMic()
{
    m_micEnabled = false;
    printf("[AUDIO] Microphone stopped\n");
}

esp_err_t AudioDriver::startSpeaker()
{
    if (m_speakerEnabled) {
        return ESP_OK;
    }

    m_speakerEnabled = true;
    printf("[AUDIO] Speaker started\n");
    return ESP_OK;
}

void AudioDriver::stopSpeaker()
{
    m_speakerEnabled = false;
    printf("[AUDIO] Speaker stopped\n");
}

int AudioDriver::readAudioData(uint8_t* buffer, int length)
{
    if (!m_micEnabled || !buffer) {
        return 0;
    }

    // Simulate reading audio data
    int bytesToRead = (length > sizeof(m_inputBuffer)) ? sizeof(m_inputBuffer) : length;
    memcpy(buffer, m_inputBuffer, bytesToRead);
    return bytesToRead;
}

void AudioDriver::writeAudioData(const uint8_t* buffer, int length)
{
    if (!m_speakerEnabled || !buffer) {
        return;
    }

    // Simulate writing audio data
    int bytesToWrite = (length > sizeof(m_outputBuffer)) ? sizeof(m_outputBuffer) : length;
    memcpy(m_outputBuffer, buffer, bytesToWrite);
}