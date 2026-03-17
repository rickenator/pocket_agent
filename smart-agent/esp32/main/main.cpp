#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Driver includes
#include "display_driver.h"
#include "voice_driver.h"
#include "audio_driver.h"
#include "ai_client.h"
#include "wifi_manager.h"
#include "web_server.h"

static const char* TAG = "MAIN";

// Ollama server URL — update to your LAN server address
#define OLLAMA_SERVER_URL "http://192.168.1.100:11434"
#define OLLAMA_MODEL      "llama3"

// Global driver instances
static DisplayDriver* g_display    = nullptr;
static VoiceDriver*   g_voice      = nullptr;
static AudioDriver*   g_audio      = nullptr;
static AIClient*      g_ai         = nullptr;
static WiFiManager*   g_wifi       = nullptr;
static WebServer*     g_web_server = nullptr;

// Event group for main application
static EventGroupHandle_t s_event_group = nullptr;
#define APP_EVENT_LISTENING_COMPLETED (1 << 0)

/**
 * @brief AI processing task — waits for voice command, runs full agent loop, displays result
 */
static void ai_processing_task(void* pvParameters)
{
    ESP_LOGI(TAG, "AI Processing Task started");

    while (1) {
        // Wait for listening to complete; clear the bit so we don't spin
        EventBits_t bits = xEventGroupWaitBits(
            s_event_group,
            APP_EVENT_LISTENING_COMPLETED,
            pdTRUE,   // clear on exit (was pdFALSE — bug fix)
            pdTRUE,
            portMAX_DELAY
        );

        if (bits & APP_EVENT_LISTENING_COMPLETED) {
            ESP_LOGI(TAG, "AI processing command...");

            const char* cmd_str = g_voice->getCommandString();
            ESP_LOGI(TAG, "Voice command: %s", cmd_str);

            // Display "Thinking..." while the agent works
            g_display->drawStatus("Thinking...", DISPLAY_COLOR_YELLOW);
            g_display->update();

            // Run the full agent loop (may involve tool calls)
            std::string response;
            esp_err_t err = g_ai->processAgentQuery(cmd_str, response);

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Agent response: %s", response.c_str());
                g_display->drawStatus(response.c_str(), DISPLAY_COLOR_WHITE);
            } else {
                ESP_LOGE(TAG, "Agent query failed: %s", esp_err_to_name(err));
                g_display->drawStatus("Error: AI request failed", DISPLAY_COLOR_RED);
            }
            g_display->update();
        }
    }
}

/**
 * @brief Goal execution task — periodically checks for pending goals and runs their subtasks
 */
static void goal_execution_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Goal Execution Task started");

    while (1) {
        GoalManager* goals = g_ai->getGoals();
        ToolRegistry* tools = g_ai->getTools();

        if (goals && tools) {
            Goal* goal = goals->getNextPendingGoal();
            if (goal) {
                ESP_LOGI(TAG, "Executing pending goal: %s", goal->description.c_str());
                // Decompose if no subtasks yet
                if (goal->subtasks.empty()) {
                    OllamaClient* ollama = g_ai->getOllama();
                    esp_err_t decErr = goals->decomposeGoal(goal, ollama, tools);
                    if (decErr != ESP_OK) {
                        ESP_LOGE(TAG, "Goal decomposition failed: %s", esp_err_to_name(decErr));
                        // Mark goal as failed so it is not retried indefinitely
                        goal->status = GoalStatus::FAILED;
                        goals->saveGoals();
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        continue;
                    }
                }
                goals->executeNextSubtask(goal, tools);
                goals->saveGoals(); // persist state
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // check every 5 seconds
    }
}

/**
 * @brief WiFi connection monitoring task
 */
static void wifi_monitor_task(void* pvParameters)
{
    ESP_LOGI(TAG, "WiFi Monitor Task started");

    while (1) {
        if (g_wifi->isConnected()) {
            const char* ip   = g_wifi->getIP().c_str();
            const char* ssid = g_wifi->getSSID().c_str();

            ESP_LOGI(TAG, "Connected to WiFi: %s, IP: %s", ssid, ip);

            g_display->clear();
            g_display->drawText(5,  0, "WiFi Connected",  DISPLAY_COLOR_GREEN);
            g_display->drawText(10, 12, ssid,              DISPLAY_COLOR_WHITE);
            g_display->drawText(10, 24, ip,                DISPLAY_COLOR_WHITE);
            g_display->drawText(5,  36, "AI Ready",        DISPLAY_COLOR_GREEN);
            g_display->update();

            if (g_web_server != nullptr) {
                g_web_server->stop();
                ESP_LOGI(TAG, "Web server stopped");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Main application task — polls voice driver and triggers AI processing
 */
static void app_main_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Application Task started");

    while (1) {
        voice_command_t cmd = g_voice->getCommand();

        if (cmd != VOICE_COMMAND_UNKNOWN) {
            ESP_LOGI(TAG, "Command received: %d", cmd);
            xEventGroupSetBits(s_event_group, APP_EVENT_LISTENING_COMPLETED);
            g_voice->stopListening();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Main entry point
 */
extern "C" void app_main()
{
    ESP_LOGI(TAG, "=== Smart Agent System Starting ===");

    // Initialize NVS flash (required for WiFiManager and MemoryManager)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated/new, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize event group
    s_event_group = xEventGroupCreate();
    if (s_event_group == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Initialize display driver for ESP32-S3-Touch-AMOLED-1.75C
    g_display = new DisplayDriver();
    if (g_display->init(DISPLAY_HARDWARE_QSPI) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display driver");
        return;
    }
    g_display->drawStatus("Initializing...", DISPLAY_COLOR_WHITE);
    g_display->update();

    // Initialize WiFi manager
    g_wifi = new WiFiManager();
    if (g_wifi->init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return;
    }

    // Auto-connect with stored credentials, or start AP provisioning
    bool use_provisioning = true;
    if (g_wifi->hasStoredCredentials()) {
        ESP_LOGI(TAG, "Found stored WiFi credentials, attempting connection");
        g_display->drawText(5,  0, "WiFi Auto-Connect",  DISPLAY_COLOR_WHITE);
        g_display->drawText(10, 12, "Connecting...",      DISPLAY_COLOR_YELLOW);
        g_display->update();

        if (g_wifi->connectWithStoredCredentials() == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            if (g_wifi->isConnected()) {
                ESP_LOGI(TAG, "Auto-connected to stored WiFi");
                use_provisioning = false;
            } else {
                ESP_LOGW(TAG, "Auto-connect failed, falling back to provisioning");
            }
        }
    }

    if (use_provisioning) {
        g_display->drawText(5,  0,  "WiFi Setup",          DISPLAY_COLOR_WHITE);
        g_display->drawText(10, 12, "Please connect",      DISPLAY_COLOR_GREEN);
        g_display->drawText(10, 24, "to 'Buddy-XXXX'",    DISPLAY_COLOR_GREEN);
        g_display->drawText(5,  36, "Then open:",          DISPLAY_COLOR_YELLOW);
        g_display->drawText(10, 48, "http://192.168.4.1", DISPLAY_COLOR_YELLOW);
        g_display->update();

        char ap_ssid[32];
        snprintf(ap_ssid, sizeof(ap_ssid), "Buddy-%04X", (uint16_t)(esp_random() % 65536));
        g_wifi->startAPMode(ap_ssid);

        g_web_server = new WebServer();
        if (g_web_server->init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize web server");
            return;
        }
        g_web_server->start(g_wifi->getAPIP().c_str());
    }

    // Initialize voice driver
    g_voice = new VoiceDriver();
    if (g_voice->init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize voice driver");
        return;
    }

    // Initialize audio driver
    g_audio = new AudioDriver();
    if (g_audio->init(AUDIO_SAMPLE_RATE_16000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio driver");
        return;
    }

    // Initialize AI client + full agent stack
    g_ai = new AIClient();
    if (g_ai->init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AI client");
        return;
    }

    // Configure agent: connect to Ollama, set up memory, goals, tools
    if (g_ai->initAgent(OLLAMA_SERVER_URL, OLLAMA_MODEL) != ESP_OK) {
        ESP_LOGW(TAG, "Agent init incomplete (Ollama may not be reachable yet)");
        // Non-fatal — requests will retry when WiFi/server is available
    }

    // Create tasks
    xTaskCreate(wifi_monitor_task,    "wifi_monitor",    2048, nullptr, 2, nullptr);
    xTaskCreate(ai_processing_task,   "ai_processing",   8192, nullptr, 5, nullptr);
    xTaskCreate(goal_execution_task,  "goal_execution",  8192, nullptr, 3, nullptr);
    xTaskCreate(app_main_task,        "app_main",        4096, nullptr, 3, nullptr);

    // Display startup screen
    g_display->clear();
    g_display->drawText(5,  0,  "Smart Agent",          DISPLAY_COLOR_WHITE);
    g_display->drawText(10, 12, "System Ready",          DISPLAY_COLOR_GREEN);
    g_display->drawText(5,  24, "Say 'Start' to begin",  DISPLAY_COLOR_YELLOW);
    g_display->drawText(5,  36, "Say 'Stop' to pause",   DISPLAY_COLOR_YELLOW);
    g_display->drawText(5,  48, "Say 'Reset' to reboot", DISPLAY_COLOR_YELLOW);
    g_display->update();

    ESP_LOGI(TAG, "=== System Started ===");
}