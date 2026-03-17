#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include <string>
#include <map>
#include <functional>
#include "esp_err.h"

struct ToolResult {
    bool success;
    std::string output;
    std::string error;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string parametersSchema;  // JSON schema string
    std::function<ToolResult(const char* paramsJson)> execute;
};

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();

    void registerTool(const ToolDefinition& tool);
    ToolResult executeTool(const char* name, const char* paramsJson);
    bool hasTool(const char* name) const;

    // Generate tool descriptions for LLM system prompt
    std::string getToolDescriptionsForPrompt() const;

    // Parse LLM response for tool calls
    // Expected format: {"tool_call":{"name":"...","parameters":{...}}}
    bool hasToolCall(const char* llmResponse) const;
    std::string extractToolName(const char* llmResponse) const;
    std::string extractToolParams(const char* llmResponse) const;

    // Register the 5 built-in tools
    void registerBuiltinTools();

private:
    std::map<std::string, ToolDefinition> m_tools;
};

// Built-in tool implementations
ToolResult tool_get_time(const char* paramsJson);
ToolResult tool_get_system_info(const char* paramsJson);
ToolResult tool_get_weather(const char* paramsJson);
ToolResult tool_set_reminder(const char* paramsJson);
ToolResult tool_control_gpio(const char* paramsJson);

#endif // TOOL_REGISTRY_H
