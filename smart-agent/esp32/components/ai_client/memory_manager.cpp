#include "memory_manager.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char* TAG = "MEMORY_MGR";
static const char* NVS_NAMESPACE = "agent_mem";
static const char* NVS_KEY_HISTORY = "conv_history";
static const char* NVS_KEY_STATE   = "agent_state";

MemoryManager::MemoryManager()
    : m_nvsHandle(0)
    , m_mutex(nullptr)
    , m_initialized(false)
{
}

MemoryManager::~MemoryManager()
{
    deinit();
}

esp_err_t MemoryManager::init()
{
    if (m_initialized) {
        return ESP_OK;
    }

    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &m_nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return err;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "MemoryManager initialized");
    return ESP_OK;
}

void MemoryManager::deinit()
{
    if (m_initialized) {
        nvs_close(m_nvsHandle);
        m_initialized = false;
    }
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

void MemoryManager::addTurn(const char* role, const char* content)
{
    if (!role || !content) return;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    // Enforce MAX_HISTORY_TURNS by removing oldest non-system turn
    while (m_history.size() >= MAX_HISTORY_TURNS) {
        // Find first non-system turn and remove it
        auto it = m_history.begin();
        while (it != m_history.end() && it->role == "system") {
            ++it;
        }
        if (it != m_history.end()) {
            m_history.erase(it);
        } else {
            // All turns are system prompts — remove oldest
            m_history.erase(m_history.begin());
        }
    }

    ConversationTurn turn;
    turn.role = role;
    turn.content = content;
    turn.timestamp = esp_timer_get_time() / 1000000LL;
    m_history.push_back(turn);

    xSemaphoreGive(m_mutex);
}

const std::vector<ConversationTurn>& MemoryManager::getHistory() const
{
    return m_history;
}

void MemoryManager::clearHistory()
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_history.clear();
    xSemaphoreGive(m_mutex);
}

esp_err_t MemoryManager::saveHistory()
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    cJSON* root = cJSON_CreateArray();
    if (!root) {
        xSemaphoreGive(m_mutex);
        return ESP_ERR_NO_MEM;
    }

    for (const auto& turn : m_history) {
        cJSON* item = cJSON_CreateObject();
        if (!item) continue;
        cJSON_AddStringToObject(item, "role", turn.role.c_str());
        cJSON_AddStringToObject(item, "content", turn.content.c_str());
        cJSON_AddNumberToObject(item, "ts", (double)turn.timestamp);
        cJSON_AddItemToArray(root, item);
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    esp_err_t err = ESP_OK;
    if (json) {
        size_t len = strlen(json);
        if (len > NVS_MAX_STRING) {
            // Truncate by trimming oldest turns
            ESP_LOGW(TAG, "History JSON too large (%u), trimming oldest turns", (unsigned)len);
            // Drop all and re-serialize with fewer entries
            cJSON* trimmed = cJSON_CreateArray();
            // Keep only the last MAX_HISTORY_TURNS/2 turns
            size_t start = m_history.size() > MAX_HISTORY_TURNS / 2
                           ? m_history.size() - MAX_HISTORY_TURNS / 2 : 0;
            for (size_t i = start; i < m_history.size(); ++i) {
                cJSON* item = cJSON_CreateObject();
                if (!item) continue;
                cJSON_AddStringToObject(item, "role", m_history[i].role.c_str());
                cJSON_AddStringToObject(item, "content", m_history[i].content.c_str());
                cJSON_AddNumberToObject(item, "ts", (double)m_history[i].timestamp);
                cJSON_AddItemToArray(trimmed, item);
            }
            free(json);
            json = cJSON_PrintUnformatted(trimmed);
            cJSON_Delete(trimmed);
        }

        if (json) {
            err = nvs_set_str(m_nvsHandle, NVS_KEY_HISTORY, json);
            if (err == ESP_OK) {
                err = nvs_commit(m_nvsHandle);
            }
            free(json);
        } else {
            err = ESP_ERR_NO_MEM;
        }
    } else {
        err = ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(m_mutex);
    return err;
}

esp_err_t MemoryManager::loadHistory()
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(m_nvsHandle, NVS_KEY_HISTORY, nullptr, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No history in NVS: %s", esp_err_to_name(err));
        return ESP_OK; // Not an error — just no history saved yet
    }

    char* json = (char*)malloc(required_size);
    if (!json) return ESP_ERR_NO_MEM;

    err = nvs_get_str(m_nvsHandle, NVS_KEY_HISTORY, json, &required_size);
    if (err != ESP_OK) {
        free(json);
        return err;
    }

    cJSON* root = cJSON_Parse(json);
    free(json);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse history JSON");
        return ESP_FAIL;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_history.clear();

    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        cJSON* role    = cJSON_GetObjectItemCaseSensitive(item, "role");
        cJSON* content = cJSON_GetObjectItemCaseSensitive(item, "content");
        cJSON* ts      = cJSON_GetObjectItemCaseSensitive(item, "ts");

        if (cJSON_IsString(role) && cJSON_IsString(content)) {
            ConversationTurn turn;
            turn.role    = role->valuestring;
            turn.content = content->valuestring;
            turn.timestamp = ts ? (int64_t)ts->valuedouble : 0;
            m_history.push_back(turn);
        }
    }

    cJSON_Delete(root);
    xSemaphoreGive(m_mutex);

    ESP_LOGI(TAG, "Loaded %u turns from NVS", (unsigned)m_history.size());
    return ESP_OK;
}

esp_err_t MemoryManager::setPreference(const char* key, const char* value)
{
    if (!m_initialized || !key || !value) return ESP_ERR_INVALID_ARG;

    // NVS keys must be ≤ 15 chars. Prefix "p_" (2 chars) leaves 13 for the key name.
    std::string nvsKey = std::string("p_") + key;
    if (nvsKey.size() > 15) {
        ESP_LOGE(TAG, "Preference key too long (max 13 chars): %s", key);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_str(m_nvsHandle, nvsKey.c_str(), value);
    if (err == ESP_OK) {
        err = nvs_commit(m_nvsHandle);
    }
    return err;
}

std::string MemoryManager::getPreference(const char* key, const char* defaultVal)
{
    if (!m_initialized || !key) return defaultVal ? defaultVal : "";

    std::string nvsKey = std::string("p_") + key;
    if (nvsKey.size() > 15) {
        ESP_LOGW(TAG, "Preference key too long: %s", key);
        return defaultVal ? defaultVal : "";
    }

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(m_nvsHandle, nvsKey.c_str(), nullptr, &required_size);
    if (err != ESP_OK) {
        return defaultVal ? defaultVal : "";
    }

    char* buf = (char*)malloc(required_size);
    if (!buf) return defaultVal ? defaultVal : "";

    err = nvs_get_str(m_nvsHandle, nvsKey.c_str(), buf, &required_size);
    std::string result = (err == ESP_OK) ? buf : (defaultVal ? defaultVal : "");
    free(buf);
    return result;
}

esp_err_t MemoryManager::saveAgentState(const char* stateJson)
{
    if (!m_initialized || !stateJson) return ESP_ERR_INVALID_ARG;

    esp_err_t err = nvs_set_str(m_nvsHandle, NVS_KEY_STATE, stateJson);
    if (err == ESP_OK) {
        err = nvs_commit(m_nvsHandle);
    }
    return err;
}

std::string MemoryManager::loadAgentState()
{
    if (!m_initialized) return "";

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(m_nvsHandle, NVS_KEY_STATE, nullptr, &required_size);
    if (err != ESP_OK) return "";

    char* buf = (char*)malloc(required_size);
    if (!buf) return "";

    err = nvs_get_str(m_nvsHandle, NVS_KEY_STATE, buf, &required_size);
    std::string result = (err == ESP_OK) ? buf : "";
    free(buf);
    return result;
}

std::string MemoryManager::buildMessagesJson(const char* systemPrompt) const
{
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return "[]";

    // Optionally prepend a system message
    if (systemPrompt && systemPrompt[0] != '\0') {
        cJSON* sys = cJSON_CreateObject();
        if (sys) {
            cJSON_AddStringToObject(sys, "role", "system");
            cJSON_AddStringToObject(sys, "content", systemPrompt);
            cJSON_AddItemToArray(arr, sys);
        }
    }

    for (const auto& turn : m_history) {
        // Skip system turns already covered above, but include "tool" turns
        cJSON* item = cJSON_CreateObject();
        if (!item) continue;
        cJSON_AddStringToObject(item, "role", turn.role.c_str());
        cJSON_AddStringToObject(item, "content", turn.content.c_str());
        cJSON_AddItemToArray(arr, item);
    }

    char* json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!json) return "[]";
    std::string result(json);
    free(json);
    return result;
}

void MemoryManager::pruneOldConversations(int64_t maxAgeSecs, size_t keepMinTurns)
{
    if (!m_mutex) return;
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    if (m_history.size() <= keepMinTurns) {
        xSemaphoreGive(m_mutex);
        return;
    }

    int64_t nowSecs = esp_timer_get_time() / 1000000LL;

    size_t removed = 0;
    // Iterate from oldest; stop once we would go below keepMinTurns
    auto it = m_history.begin();
    while (it != m_history.end() && (m_history.size() - removed) > keepMinTurns) {
        if (maxAgeSecs > 0 && it->timestamp > 0 &&
            (nowSecs - it->timestamp) > maxAgeSecs) {
            it = m_history.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    xSemaphoreGive(m_mutex);

    if (removed > 0) {
        ESP_LOGI(TAG, "Pruned %u old conversation turn(s)", (unsigned)removed);
    }
}

size_t MemoryManager::estimateTokenCount(const std::string& text)
{
    // Rough approximation: ~4 characters per token (English text, GPT/Llama convention)
    return (text.size() + 3) / 4;
}

size_t MemoryManager::totalHistoryTokens() const
{
    size_t total = 0;
    for (const auto& turn : m_history) {
        total += estimateTokenCount(turn.content);
    }
    return total;
}
