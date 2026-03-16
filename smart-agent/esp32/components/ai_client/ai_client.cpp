#include "ai_client.h"
#include <stdio.h>
#include <string.h>

AIClient::AIClient()
    : m_initialized(false)
{
    memset(m_response, 0, sizeof(m_response));
    m_responseType = AI_RESPONSE_TEXT;
}

AIClient::~AIClient()
{
    deinit();
}

esp_err_t AIClient::init()
{
    if (m_initialized) {
        return ESP_OK;
    }

    m_initialized = true;
    printf("[AI] AI client initialized\n");
    return ESP_OK;
}

void AIClient::deinit()
{
    if (m_initialized) {
        printf("[AI] AI client deinitialized\n");
    }
    m_initialized = false;
}

esp_err_t AIClient::processVoiceCommand(const char* command)
{
    if (!m_initialized || !command) {
        return ESP_ERR_INVALID_ARG;
    }

    printf("[AI] Processing voice command: %s\n", command);

    // Simulate AI processing
    const char* responses[] = {
        "Processing voice command...",
        "Analyzing command...",
        "Executing action..."
    };

    const char* response = responses[rand() % 3];
    strncpy(m_response, response, sizeof(m_response) - 1);
    m_responseType = AI_RESPONSE_ACTION;
    m_response[sizeof(m_response) - 1] = '\0';

    return ESP_OK;
}

esp_err_t AIClient::processTextQuery(const char* query)
{
    if (!m_initialized || !query) {
        return ESP_ERR_INVALID_ARG;
    }

    printf("[AI] Processing text query: %s\n", query);

    // Simulate AI processing
    const char* responses[] = {
        "Searching database...",
        "Generating response...",
        "Query complete"
    };

    const char* response = responses[rand() % 3];
    strncpy(m_response, response, sizeof(m_response) - 1);
    m_responseType = AI_RESPONSE_TEXT;
    m_response[sizeof(m_response) - 1] = '\0';

    return ESP_OK;
}

const char* AIClient::getResponse()
{
    return m_response;
}

ai_response_type_t AIClient::getResponseType()
{
    return m_responseType;
}