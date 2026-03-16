#include "voice_driver.h"
#include <stdio.h>
#include <string.h>

VoiceDriver::VoiceDriver()
    : m_isListening(false)
    , m_lastCommand(VOICE_COMMAND_UNKNOWN)
{
}

VoiceDriver::~VoiceDriver()
{
    deinit();
}

esp_err_t VoiceDriver::init()
{
    m_isListening = false;
    m_lastCommand = VOICE_COMMAND_UNKNOWN;
    printf("[VOICE] Voice driver initialized\n");
    return ESP_OK;
}

void VoiceDriver::deinit()
{
    if (m_isListening) {
        stopListening();
    }
    printf("[VOICE] Voice driver deinitialized\n");
}

esp_err_t VoiceDriver::startListening()
{
    if (m_isListening) {
        return ESP_OK;
    }

    m_isListening = true;
    printf("[VOICE] Listening for commands...\n");
    return ESP_OK;
}

void VoiceDriver::stopListening()
{
    m_isListening = false;
    printf("[VOICE] Listening stopped\n");
}

voice_command_t VoiceDriver::getCommand()
{
    voice_command_t cmd = m_lastCommand;
    m_lastCommand = VOICE_COMMAND_UNKNOWN;
    return cmd;
}

const char* VoiceDriver::getCommandString()
{
    switch (m_lastCommand) {
        case VOICE_COMMAND_START:
            return "START";
        case VOICE_COMMAND_STOP:
            return "STOP";
        case VOICE_COMMAND_RESET:
            return "RESET";
        case VOICE_COMMAND_QUERY:
            return "QUERY";
        case VOICE_COMMAND_CONFIG:
            return "CONFIG";
        default:
            return "UNKNOWN";
    }
}