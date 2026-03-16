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

    // Get detected command
    voice_command_t getCommand();
    const char* getCommandString();

private:
    bool m_isListening;
    voice_command_t m_lastCommand;
};

#endif // VOICE_DRIVER_H