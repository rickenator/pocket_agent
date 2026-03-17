#ifndef VOICE_DRIVER_H
#define VOICE_DRIVER_H

#include <stdint.h>
#include <string.h>
#include "esp_err.h"

// Voice command types
typedef enum {
    VOICE_COMMAND_UNKNOWN = 0,
    VOICE_COMMAND_START,
    VOICE_COMMAND_STOP,
    VOICE_COMMAND_RESET,
    VOICE_COMMAND_QUERY,
    VOICE_COMMAND_CONFIG,
} voice_command_t;

// Voice driver class
class VoiceDriver {
public:
    VoiceDriver();
    ~VoiceDriver();

    esp_err_t init();
    void deinit();

    // Speech recognition
    esp_err_t startListening();
    void stopListening();

    // Set the raw text that was captured from voice input (e.g., from STT)
    void setCommandText(const char* text);

    // Get detected command enum (clears it after reading)
    voice_command_t getCommand();
    // Get the raw text for the last voice command (does NOT clear — stable until next command)
    const char* getCommandString() const;

private:
    bool m_isListening;
    voice_command_t m_lastCommand;
    char m_commandText[512];  // Raw text from STT for the current command
};

#endif // VOICE_DRIVER_H