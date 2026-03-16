#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <stdint.h>
#include <string.h>
#include "esp_err.h"

// AI response types
typedef enum {
    AI_RESPONSE_TEXT = 0,
    AI_RESPONSE_ACTION,
    AI_RESPONSE_VOICE,
} ai_response_type_t;

// AI client class
class AIClient {
public:
    AIClient();
    ~AIClient();

    esp_err_t init();
    void deinit();

    // AI interaction
    esp_err_t processVoiceCommand(const char* command);
    esp_err_t processTextQuery(const char* query);

    // Get response
    const char* getResponse();
    ai_response_type_t getResponseType();

private:
    char m_response[512];
    ai_response_type_t m_responseType;
    bool m_initialized;
};

#endif // AI_CLIENT_H