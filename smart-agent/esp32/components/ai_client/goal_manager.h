#ifndef GOAL_MANAGER_H
#define GOAL_MANAGER_H

#include <string>
#include <vector>
#include "esp_err.h"
#include "memory_manager.h"

enum class GoalStatus   { PENDING, ACTIVE, COMPLETED, FAILED };
enum class GoalPriority { LOW, MEDIUM, HIGH, CRITICAL };

struct SubTask {
    std::string description;
    std::string toolName;
    std::string parameters;  // JSON string
    GoalStatus status;
    std::string result;
};

struct Goal {
    std::string id;           // "goal_<timestamp>"
    std::string description;
    GoalPriority priority;
    GoalStatus status;
    std::vector<SubTask> subtasks;
    int64_t createdAt;        // seconds since boot
};

class GoalManager {
public:
    GoalManager();
    ~GoalManager();

    esp_err_t init(MemoryManager* memory);

    std::string addGoal(const char* description, GoalPriority priority = GoalPriority::MEDIUM);
    esp_err_t removeGoal(const char* goalId);
    Goal* getNextPendingGoal();
    const std::vector<Goal>& getGoals() const;

    // LLM-assisted decomposition: ask Ollama to break goal into subtasks
    esp_err_t decomposeGoal(Goal* goal, class OllamaClient* ollama, class ToolRegistry* tools);

    // Execute next PENDING subtask of a goal via ToolRegistry
    esp_err_t executeNextSubtask(Goal* goal, class ToolRegistry* tools);

    // Persistence via MemoryManager NVS
    esp_err_t saveGoals();
    esp_err_t loadGoals();

private:
    std::vector<Goal> m_goals;
    MemoryManager* m_memory;

    void sortByPriority();
    std::string generateGoalId();
};

#endif // GOAL_MANAGER_H
