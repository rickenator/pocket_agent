#include "goal_manager.h"
#include "ollama_client.h"
#include "tool_registry.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cstdio>

static const char* TAG = "GOAL_MGR";

GoalManager::GoalManager()
    : m_memory(nullptr)
{
}

GoalManager::~GoalManager()
{
}

esp_err_t GoalManager::init(MemoryManager* memory)
{
    m_memory = memory;
    ESP_LOGI(TAG, "GoalManager initialized");
    return ESP_OK;
}

std::string GoalManager::generateGoalId()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "goal_%lld", (long long)(esp_timer_get_time() / 1000000LL));
    return buf;
}

std::string GoalManager::addGoal(const char* description, GoalPriority priority)
{
    if (!description) return "";

    Goal g;
    g.id          = generateGoalId();
    g.description = description;
    g.priority    = priority;
    g.status      = GoalStatus::PENDING;
    g.createdAt   = esp_timer_get_time() / 1000000LL;

    m_goals.push_back(g);
    sortByPriority();

    ESP_LOGI(TAG, "Goal added: %s (%s)", g.id.c_str(), description);
    return g.id;
}

esp_err_t GoalManager::removeGoal(const char* goalId)
{
    if (!goalId) return ESP_ERR_INVALID_ARG;

    auto it = std::remove_if(m_goals.begin(), m_goals.end(),
                             [goalId](const Goal& g){ return g.id == goalId; });
    if (it == m_goals.end()) {
        return ESP_ERR_NOT_FOUND;
    }
    m_goals.erase(it, m_goals.end());
    return ESP_OK;
}

Goal* GoalManager::getNextPendingGoal()
{
    for (auto& g : m_goals) {
        if (g.status == GoalStatus::PENDING) {
            return &g;
        }
    }
    return nullptr;
}

const std::vector<Goal>& GoalManager::getGoals() const
{
    return m_goals;
}

void GoalManager::sortByPriority()
{
    std::sort(m_goals.begin(), m_goals.end(), [](const Goal& a, const Goal& b){
        return static_cast<int>(a.priority) > static_cast<int>(b.priority);
    });
}

esp_err_t GoalManager::decomposeGoal(Goal* goal, OllamaClient* ollama, ToolRegistry* tools)
{
    if (!goal || !ollama || !tools) return ESP_ERR_INVALID_ARG;

    // Build the decomposition prompt
    std::string toolDescs = tools->getToolDescriptionsForPrompt();
    std::string prompt =
        "You are a task planner. Break down the following goal into a numbered list of subtasks. "
        "For each subtask, specify the tool to use and the parameters as JSON.\n"
        "Available tools:\n" + toolDescs + "\n\n"
        "Goal: " + goal->description + "\n\n"
        "Respond ONLY with valid JSON in this exact format (an array):\n"
        "[{\"description\":\"...\",\"tool\":\"tool_name\",\"parameters\":{...}},...]";

    std::string response;
    esp_err_t err = ollama->createChatCompletion(prompt.c_str(), response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LLM decomposition failed: %s", esp_err_to_name(err));
        return err;
    }

    // Find the JSON array in the response
    size_t start = response.find('[');
    size_t end   = response.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        ESP_LOGE(TAG, "No JSON array in decomposition response");
        return ESP_FAIL;
    }

    std::string jsonStr = response.substr(start, end - start + 1);
    cJSON* arr = cJSON_Parse(jsonStr.c_str());
    if (!arr) {
        ESP_LOGE(TAG, "Failed to parse decomposition JSON");
        return ESP_FAIL;
    }

    goal->subtasks.clear();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, arr) {
        cJSON* desc   = cJSON_GetObjectItemCaseSensitive(item, "description");
        cJSON* tool   = cJSON_GetObjectItemCaseSensitive(item, "tool");
        cJSON* params = cJSON_GetObjectItemCaseSensitive(item, "parameters");

        if (!cJSON_IsString(desc) || !cJSON_IsString(tool)) continue;

        SubTask sub;
        sub.description = desc->valuestring;
        sub.toolName    = tool->valuestring;
        sub.status      = GoalStatus::PENDING;

        if (params) {
            char* p = cJSON_PrintUnformatted(params);
            if (p) { sub.parameters = p; free(p); }
        } else {
            sub.parameters = "{}";
        }

        goal->subtasks.push_back(sub);
    }
    cJSON_Delete(arr);

    ESP_LOGI(TAG, "Goal %s decomposed into %u subtasks",
             goal->id.c_str(), (unsigned)goal->subtasks.size());
    goal->status = GoalStatus::ACTIVE;
    return ESP_OK;
}

esp_err_t GoalManager::executeNextSubtask(Goal* goal, ToolRegistry* tools)
{
    if (!goal || !tools) return ESP_ERR_INVALID_ARG;

    for (auto& sub : goal->subtasks) {
        if (sub.status != GoalStatus::PENDING) continue;

        ESP_LOGI(TAG, "Executing subtask: %s via tool '%s'",
                 sub.description.c_str(), sub.toolName.c_str());

        sub.status = GoalStatus::ACTIVE;
        ToolResult result = tools->executeTool(sub.toolName.c_str(), sub.parameters.c_str());

        if (result.success) {
            sub.status = GoalStatus::COMPLETED;
            sub.result = result.output;
            ESP_LOGI(TAG, "Subtask completed: %s", result.output.c_str());
        } else {
            sub.status = GoalStatus::FAILED;
            sub.result = result.error;
            ESP_LOGW(TAG, "Subtask failed: %s", result.error.c_str());
        }

        // Check if all subtasks done
        bool allDone = true;
        bool anyFailed = false;
        for (const auto& s : goal->subtasks) {
            if (s.status == GoalStatus::PENDING || s.status == GoalStatus::ACTIVE) {
                allDone = false;
            }
            if (s.status == GoalStatus::FAILED) {
                anyFailed = true;
            }
        }
        if (allDone) {
            goal->status = anyFailed ? GoalStatus::FAILED : GoalStatus::COMPLETED;
            ESP_LOGI(TAG, "Goal %s %s", goal->id.c_str(),
                     anyFailed ? "FAILED" : "COMPLETED");
        }

        return result.success ? ESP_OK : ESP_FAIL;
    }

    // No pending subtasks
    return ESP_ERR_NOT_FOUND;
}

esp_err_t GoalManager::saveGoals()
{
    if (!m_memory) return ESP_ERR_INVALID_STATE;

    cJSON* arr = cJSON_CreateArray();
    if (!arr) return ESP_ERR_NO_MEM;

    for (const auto& g : m_goals) {
        cJSON* obj = cJSON_CreateObject();
        if (!obj) continue;
        cJSON_AddStringToObject(obj, "id", g.id.c_str());
        cJSON_AddStringToObject(obj, "description", g.description.c_str());
        cJSON_AddNumberToObject(obj, "priority", (int)g.priority);
        cJSON_AddNumberToObject(obj, "status", (int)g.status);
        cJSON_AddNumberToObject(obj, "createdAt", (double)g.createdAt);

        cJSON* subtasks = cJSON_CreateArray();
        for (const auto& s : g.subtasks) {
            cJSON* sobj = cJSON_CreateObject();
            if (!sobj) continue;
            cJSON_AddStringToObject(sobj, "description", s.description.c_str());
            cJSON_AddStringToObject(sobj, "toolName", s.toolName.c_str());
            cJSON_AddStringToObject(sobj, "parameters", s.parameters.c_str());
            cJSON_AddNumberToObject(sobj, "status", (int)s.status);
            cJSON_AddStringToObject(sobj, "result", s.result.c_str());
            cJSON_AddItemToArray(subtasks, sobj);
        }
        cJSON_AddItemToObject(obj, "subtasks", subtasks);
        cJSON_AddItemToArray(arr, obj);
    }

    char* json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return ESP_ERR_NO_MEM;

    esp_err_t err = m_memory->saveAgentState(json);
    free(json);
    return err;
}

esp_err_t GoalManager::loadGoals()
{
    if (!m_memory) return ESP_ERR_INVALID_STATE;

    std::string state = m_memory->loadAgentState();
    if (state.empty()) return ESP_OK;

    cJSON* arr = cJSON_Parse(state.c_str());
    if (!arr) {
        ESP_LOGE(TAG, "Failed to parse saved goals JSON");
        return ESP_FAIL;
    }

    m_goals.clear();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, arr) {
        Goal g;
        cJSON* id   = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON* desc = cJSON_GetObjectItemCaseSensitive(item, "description");
        cJSON* pri  = cJSON_GetObjectItemCaseSensitive(item, "priority");
        cJSON* stat = cJSON_GetObjectItemCaseSensitive(item, "status");
        cJSON* cat  = cJSON_GetObjectItemCaseSensitive(item, "createdAt");

        if (!cJSON_IsString(id) || !cJSON_IsString(desc)) continue;

        g.id          = id->valuestring;
        g.description = desc->valuestring;
        g.priority    = pri  ? (GoalPriority)(int)pri->valuedouble  : GoalPriority::MEDIUM;
        g.status      = stat ? (GoalStatus)(int)stat->valuedouble   : GoalStatus::PENDING;
        g.createdAt   = cat  ? (int64_t)cat->valuedouble            : 0;

        cJSON* subtasks = cJSON_GetObjectItemCaseSensitive(item, "subtasks");
        if (cJSON_IsArray(subtasks)) {
            cJSON* sitem = nullptr;
            cJSON_ArrayForEach(sitem, subtasks) {
                SubTask s;
                cJSON* sdesc   = cJSON_GetObjectItemCaseSensitive(sitem, "description");
                cJSON* stool   = cJSON_GetObjectItemCaseSensitive(sitem, "toolName");
                cJSON* sparams = cJSON_GetObjectItemCaseSensitive(sitem, "parameters");
                cJSON* sstat   = cJSON_GetObjectItemCaseSensitive(sitem, "status");
                cJSON* sresult = cJSON_GetObjectItemCaseSensitive(sitem, "result");

                if (cJSON_IsString(sdesc)) s.description = sdesc->valuestring;
                if (cJSON_IsString(stool)) s.toolName    = stool->valuestring;
                if (cJSON_IsString(sparams)) s.parameters = sparams->valuestring;
                s.status = sstat ? (GoalStatus)(int)sstat->valuedouble : GoalStatus::PENDING;
                if (cJSON_IsString(sresult)) s.result = sresult->valuestring;
                g.subtasks.push_back(s);
            }
        }

        m_goals.push_back(g);
    }
    cJSON_Delete(arr);

    sortByPriority();
    ESP_LOGI(TAG, "Loaded %u goals from NVS", (unsigned)m_goals.size());
    return ESP_OK;
}
