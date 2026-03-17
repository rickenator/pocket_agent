#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <stdint.h>
#include <string>
#include "esp_err.h"
#include "ollama_client.h"
#include "memory_manager.h"
#include "goal_manager.h"
#include "tool_registry.h"

// AI response types
typedef enum {
    AI_RESPONSE_TEXT = 0,
    AI_RESPONSE_ACTION,
    AI_RESPONSE_VOICE,
    AI_RESPONSE_TOOL_CALL,
} ai_response_type_t;

// AI client — acts as the top-level agent orchestrator
class AIClient {
public:
    AIClient();
    ~AIClient();

    esp_err_t init();
    void deinit();

    // Full agent initialization: connects Ollama, memory, goals, tools
    esp_err_t initAgent(const char* ollamaUrl, const char* model);

    // Agent query: implements the full tool-call loop
    //   1. Add userInput to memory
    //   2. Build system prompt with tool descriptions
    //   3. Send to Ollama (with conversation history)
    //   4. If LLM returns a tool call, execute tool and re-query
    //   5. Repeat until final text response; save to memory
    esp_err_t processAgentQuery(const char* userInput, std::string& response);

    // Legacy interface — delegate to processAgentQuery
    esp_err_t processVoiceCommand(const char* command);
    esp_err_t processTextQuery(const char* query);

    // Get last response (legacy callers)
    const char* getResponse();
    ai_response_type_t getResponseType();

    // Accessors for sub-components
    OllamaClient*  getOllama()  { return m_ollama;  }
    MemoryManager* getMemory()  { return m_memory;  }
    GoalManager*   getGoals()   { return m_goals;   }
    ToolRegistry*  getTools()   { return m_tools;   }

private:
    OllamaClient*  m_ollama;
    MemoryManager* m_memory;
    GoalManager*   m_goals;
    ToolRegistry*  m_tools;

    std::string        m_lastResponse;
    ai_response_type_t m_responseType;
    bool               m_initialized;
    bool               m_agentReady;

    static const int MAX_TOOL_ITERATIONS = 5;

    std::string buildSystemPrompt() const;
};

#endif // AI_CLIENT_H