#include "voice_driver.h"
#include <stdio.h>
#include <string.h>

VoiceDriver::VoiceDriver()
    : m_isListening(false)
    , m_lastCommand(VOICE_COMMAND_UNKNOWN)
{
    m_commandText[0] = '\0';
}

VoiceDriver::~VoiceDriver()
{
    deinit();
}

esp_err_t VoiceDriver::init()
{
    m_isListening = false;
    m_lastCommand = VOICE_COMMAND_UNKNOWN;
    m_commandText[0] = '\0';
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

void VoiceDriver::setCommandText(const char* text)
{
    if (text) {
        strncpy(m_commandText, text, sizeof(m_commandText) - 1);
        m_commandText[sizeof(m_commandText) - 1] = '\0';
    } else {
        m_commandText[0] = '\0';
    }
}

const char* VoiceDriver::getCommandString() const
{
    // Return the raw STT text if available; otherwise fall back to the command enum name.
    if (m_commandText[0] != '\0') {
        return m_commandText;
    }
    switch (m_lastCommand) {
        case VOICE_COMMAND_START:   return "START";
        case VOICE_COMMAND_STOP:    return "STOP";
        case VOICE_COMMAND_RESET:   return "RESET";
        case VOICE_COMMAND_QUERY:   return "QUERY";
        case VOICE_COMMAND_CONFIG:  return "CONFIG";
        default:                    return "UNKNOWN";
    }
}