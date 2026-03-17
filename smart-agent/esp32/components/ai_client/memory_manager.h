#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <string>
#include <vector>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct ConversationTurn {
    std::string role;      // "user", "assistant", "system", "tool"
    std::string content;
    int64_t timestamp;     // seconds since boot (esp_timer_get_time() / 1000000)
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    esp_err_t init();
    void deinit();

    // Conversation history (RAM, optionally persisted)
    void addTurn(const char* role, const char* content);
    const std::vector<ConversationTurn>& getHistory() const;
    void clearHistory();
    esp_err_t saveHistory();   // Serialize to NVS as JSON
    esp_err_t loadHistory();   // Deserialize from NVS

    // User preferences (key-value, persisted in NVS)
    esp_err_t setPreference(const char* key, const char* value);
    std::string getPreference(const char* key, const char* defaultVal = "");

    // Agent state persistence
    esp_err_t saveAgentState(const char* stateJson);
    std::string loadAgentState();

    // Build JSON messages array for Ollama API
    // Returns: [{"role":"system","content":"..."},{"role":"user","content":"..."},...]
    std::string buildMessagesJson(const char* systemPrompt = nullptr) const;

private:
    nvs_handle_t m_nvsHandle;
    std::vector<ConversationTurn> m_history;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;

    static const size_t MAX_HISTORY_TURNS = 20;
    static const size_t NVS_MAX_STRING = 3900; // NVS string limit ~4000 bytes
};

#endif // MEMORY_MANAGER_H
