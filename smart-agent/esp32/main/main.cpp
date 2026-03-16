#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
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

// Global driver instances
static DisplayDriver* g_display = nullptr;
static VoiceDriver* g_voice = nullptr;
static AudioDriver* g_audio = nullptr;
static AIClient* g_ai = nullptr;
static WiFiManager* g_wifi = nullptr;
static WebServer* g_web_server = nullptr;

// Event group for main application
static EventGroupHandle_t s_event_group = nullptr;
#define APP_EVENT_LISTENING_COMPLETED (1 << 0)

/**
 * @brief AI processing task
 */
static void ai_processing_task(void* pvParameters)
{
    ESP_LOGI(TAG, "AI Processing Task started");

    while (1) {
        // Wait for listening to complete
        EventBits_t bits = xEventGroupWaitBits(
            s_event_group,
            APP_EVENT_LISTENING_COMPLETED,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );

        if (bits & APP_EVENT_LISTENING_COMPLETED) {
            ESP_LOGI(TAG, "AI processing command...");

            voice_command_t cmd = g_voice->getCommand();
            const char* cmd_str = g_voice->getCommandString();

            ESP_LOGI(TAG, "Voice command detected: %s", cmd_str);

            // Process the command with AI
            g_ai->processVoiceCommand(cmd_str);

            const char* response = g_ai->getResponse();
            ai_response_type_t response_type = g_ai->getResponseType();

            ESP_LOGI(TAG, "AI response: %s (type: %d)", response, response_type);

            // Display the response
            g_display->drawStatus(response, DISPLAY_COLOR_WHITE);
            g_display->update();
        }
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
            const char* ip = g_wifi->getIP().c_str();
            const char* ssid = g_wifi->getSSID().c_str();

            ESP_LOGI(TAG, "Connected to WiFi: %s, IP: %s", ssid, ip);

            // Display WiFi connection info
            g_display->clear();
            g_display->drawText(5, 0, "WiFi Connected", DISPLAY_COLOR_GREEN);
            g_display->drawText(10, 12, ssid, DISPLAY_COLOR_WHITE);
            g_display->drawText(10, 24, ip, DISPLAY_COLOR_WHITE);
            g_display->drawText(5, 36, "AI Ready", DISPLAY_COLOR_GREEN);
            g_display->update();

            // Stop web server when connected
            if (g_web_server != nullptr) {
                g_web_server->stop();
                ESP_LOGI(TAG, "Web server stopped");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Main application task
 */
static void app_main_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Application Task started");

    while (1) {
        // Check for voice commands periodically
        voice_command_t cmd = g_voice->getCommand();

        if (cmd != VOICE_COMMAND_UNKNOWN) {
            ESP_LOGI(TAG, "Command received: %d", cmd);

            // Start AI processing
            xEventGroupSetBits(s_event_group, APP_EVENT_LISTENING_COMPLETED);

            // Clear command after processing
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

    // Initialize event group
    s_event_group = xEventGroupCreate();
    if (s_event_group == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Initialize display driver for ESP32-S3-Touch-AMOLED-1.75C
    g_display = new DisplayDriver();
    // Use QSPI hardware mode for the AMOLED display
    // Change to DISPLAY_SIMULATION for testing without hardware
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

    // Check for stored WiFi credentials
    bool use_provisioning = true;
    if (g_wifi->hasStoredCredentials()) {
        ESP_LOGI(TAG, "Found stored WiFi credentials, attempting connection");
        g_display->drawText(5, 0, "WiFi Auto-Connect", DISPLAY_COLOR_WHITE);
        g_display->drawText(10, 12, "Connecting...", DISPLAY_COLOR_YELLOW);
        g_display->update();

        esp_err_t ret = g_wifi->connectWithStoredCredentials();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Auto-connect initiated");
            // Wait briefly for connection
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
        // Start AP mode for provisioning
        g_display->drawText(5, 0, "WiFi Setup", DISPLAY_COLOR_WHITE);
        g_display->drawText(10, 12, "Please connect", DISPLAY_COLOR_GREEN);
        g_display->drawText(10, 24, "to 'Buddy-XXXX'", DISPLAY_COLOR_GREEN);
        g_display->drawText(5, 36, "Then open:", DISPLAY_COLOR_YELLOW);
        g_display->drawText(10, 48, "http://192.168.4.1", DISPLAY_COLOR_YELLOW);
        g_display->update();

        // Start AP mode with random suffix
        char ap_ssid[32];
        snprintf(ap_ssid, sizeof(ap_ssid), "Buddy-%04X", (uint16_t)(esp_random() % 65536));
        g_wifi->startAPMode(ap_ssid);

        // Initialize web server
        g_web_server = new WebServer();
        if (g_web_server->init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize web server");
            return;
        }

        // Start web server on AP IP
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

    // Initialize AI client
    g_ai = new AIClient();
    if (g_ai->init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AI client");
        return;
    }

    // Create WiFi monitor task
    xTaskCreate(wifi_monitor_task, "wifi_monitor_task", 2048, nullptr, 2, nullptr);

    // Display startup screen
    g_display->clear();
    g_display->drawText(5, 0, "Smart Agent", DISPLAY_COLOR_WHITE);
    g_display->drawText(10, 12, "System Ready", DISPLAY_COLOR_GREEN);
    g_display->drawText(5, 24, "Say 'Start' to begin", DISPLAY_COLOR_YELLOW);
    g_display->drawText(5, 36, "Say 'Stop' to pause", DISPLAY_COLOR_YELLOW);
    g_display->drawText(5, 48, "Say 'Reset' to reboot", DISPLAY_COLOR_YELLOW);
    g_display->update();

    ESP_LOGI(TAG, "All drivers initialized successfully");

    // Create AI processing task
    xTaskCreate(ai_processing_task, "ai_processing_task", 4096, nullptr, 5, nullptr);

    // Create main application task
    xTaskCreate(app_main_task, "app_main_task", 4096, nullptr, 3, nullptr);

    ESP_LOGI(TAG, "=== System Started ===");
}