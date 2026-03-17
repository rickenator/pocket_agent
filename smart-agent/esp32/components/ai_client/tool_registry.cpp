#include "tool_registry.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "TOOL_REG";

// ---------------------------------------------------------------------------
// ToolRegistry
// ---------------------------------------------------------------------------

ToolRegistry::ToolRegistry()
{
}

ToolRegistry::~ToolRegistry()
{
}

void ToolRegistry::registerTool(const ToolDefinition& tool)
{
    m_tools[tool.name] = tool;
    ESP_LOGI(TAG, "Tool registered: %s", tool.name.c_str());
}

ToolResult ToolRegistry::executeTool(const char* name, const char* paramsJson)
{
    if (!name) return {false, "", "null tool name"};

    auto it = m_tools.find(name);
    if (it == m_tools.end()) {
        ESP_LOGW(TAG, "Unknown tool: %s", name);
        return {false, "", std::string("Unknown tool: ") + name};
    }

    ESP_LOGI(TAG, "Executing tool: %s  params: %s", name, paramsJson ? paramsJson : "{}");
    return it->second.execute(paramsJson ? paramsJson : "{}");
}

bool ToolRegistry::hasTool(const char* name) const
{
    if (!name) return false;
    return m_tools.find(name) != m_tools.end();
}

std::string ToolRegistry::getToolDescriptionsForPrompt() const
{
    std::string out;
    for (const auto& kv : m_tools) {
        out += "- " + kv.second.name + ": " + kv.second.description;
        if (!kv.second.parametersSchema.empty()) {
            out += " (params: " + kv.second.parametersSchema + ")";
        }
        out += "\n";
    }
    return out;
}

bool ToolRegistry::hasToolCall(const char* llmResponse) const
{
    if (!llmResponse) return false;
    return strstr(llmResponse, "tool_call") != nullptr;
}

std::string ToolRegistry::extractToolName(const char* llmResponse) const
{
    if (!llmResponse) return "";

    // Find JSON object containing "tool_call"
    const char* start = strstr(llmResponse, "{");
    if (!start) return "";

    cJSON* root = cJSON_Parse(start);
    if (!root) {
        // Try to find just the tool_call sub-object
        const char* tc = strstr(llmResponse, "\"tool_call\"");
        if (!tc) return "";
        // Scan to the first { after "tool_call":
        const char* brace = strchr(tc + 11, '{');
        if (!brace) return "";
        cJSON* tc_obj = cJSON_Parse(brace);
        if (!tc_obj) return "";
        cJSON* name = cJSON_GetObjectItemCaseSensitive(tc_obj, "name");
        std::string result = (cJSON_IsString(name)) ? name->valuestring : "";
        cJSON_Delete(tc_obj);
        return result;
    }

    cJSON* tc_obj = cJSON_GetObjectItemCaseSensitive(root, "tool_call");
    std::string result;
    if (tc_obj) {
        cJSON* name = cJSON_GetObjectItemCaseSensitive(tc_obj, "name");
        if (cJSON_IsString(name)) result = name->valuestring;
    }
    cJSON_Delete(root);
    return result;
}

std::string ToolRegistry::extractToolParams(const char* llmResponse) const
{
    if (!llmResponse) return "{}";

    const char* start = strstr(llmResponse, "{");
    if (!start) return "{}";

    cJSON* root = cJSON_Parse(start);
    if (!root) return "{}";

    cJSON* tc_obj = cJSON_GetObjectItemCaseSensitive(root, "tool_call");
    std::string result = "{}";
    if (tc_obj) {
        cJSON* params = cJSON_GetObjectItemCaseSensitive(tc_obj, "parameters");
        if (params) {
            char* p = cJSON_PrintUnformatted(params);
            if (p) { result = p; free(p); }
        }
    }
    cJSON_Delete(root);
    return result;
}

// ---------------------------------------------------------------------------
// Built-in tool: get_time
// ---------------------------------------------------------------------------
ToolResult tool_get_time(const char* /*paramsJson*/)
{
    int64_t us = esp_timer_get_time();
    int64_t seconds = us / 1000000LL;
    char buf[64];
    snprintf(buf, sizeof(buf), "Uptime: %lldh %lldm %llds",
             seconds / 3600, (seconds % 3600) / 60, seconds % 60);
    return {true, buf, ""};
}

// ---------------------------------------------------------------------------
// Built-in tool: get_system_info
// ---------------------------------------------------------------------------
ToolResult tool_get_system_info(const char* /*paramsJson*/)
{
    uint32_t freeHeap  = esp_get_free_heap_size();
    uint32_t minHeap   = esp_get_minimum_free_heap_size();
    int64_t  uptimeSec = esp_timer_get_time() / 1000000LL;

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"free_heap\":%lu,\"min_heap\":%lu,\"uptime_sec\":%lld,"
             "\"cores\":%d,\"revision\":%d}",
             (unsigned long)freeHeap, (unsigned long)minHeap, uptimeSec,
             chip.cores, chip.revision);
    return {true, buf, ""};
}

// ---------------------------------------------------------------------------
// Built-in tool: get_weather
// HTTP GET https://wttr.in/{city}?format=3 (plain text, small response)
// ---------------------------------------------------------------------------

struct WeatherBuf {
    char data[512];
    int  len;
};

static esp_err_t weather_http_event(esp_http_client_event_t* evt)
{
    WeatherBuf* wb = (WeatherBuf*)evt->user_data;
    if (!wb) return ESP_OK;
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
        int room = (int)sizeof(wb->data) - wb->len - 1;
        if (room > 0) {
            int copy = evt->data_len < room ? evt->data_len : room;
            memcpy(wb->data + wb->len, evt->data, copy);
            wb->len += copy;
            wb->data[wb->len] = '\0';
        }
    }
    return ESP_OK;
}

ToolResult tool_get_weather(const char* paramsJson)
{
    cJSON* params = cJSON_Parse(paramsJson);
    std::string city = "London"; // default
    if (params) {
        cJSON* c = cJSON_GetObjectItemCaseSensitive(params, "city");
        if (cJSON_IsString(c) && c->valuestring) city = c->valuestring;
        cJSON_Delete(params);
    }

    // Replace spaces with + for URL
    for (char& ch : city) { if (ch == ' ') ch = '+'; }

    std::string url = "http://wttr.in/" + city + "?format=3";

    WeatherBuf wb = {};
    esp_http_client_config_t config = {};
    config.url         = url.c_str();
    config.method      = HTTP_METHOD_GET;
    config.timeout_ms  = 15000;
    config.event_handler = weather_http_event;
    config.user_data   = &wb;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return {false, "", "Failed to init HTTP client"};

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        return {false, "", std::string("Weather fetch failed: ") + esp_err_to_name(err)};
    }

    return {true, std::string(wb.data), ""};
}

// ---------------------------------------------------------------------------
// Built-in tool: set_reminder
// Creates a one-shot FreeRTOS timer that logs after delay_seconds.
// ---------------------------------------------------------------------------

struct ReminderData {
    char message[128];
};

static void reminder_callback(TimerHandle_t xTimer)
{
    ReminderData* rd = (ReminderData*)pvTimerGetTimerID(xTimer);
    if (rd) {
        ESP_LOGI(TAG, "⏰ REMINDER: %s", rd->message);
        free(rd);
    }
    xTimerDelete(xTimer, 0);
}

ToolResult tool_set_reminder(const char* paramsJson)
{
    cJSON* params = cJSON_Parse(paramsJson);
    if (!params) return {false, "", "Invalid JSON params"};

    cJSON* msg  = cJSON_GetObjectItemCaseSensitive(params, "message");
    cJSON* del  = cJSON_GetObjectItemCaseSensitive(params, "delay_seconds");

    if (!cJSON_IsString(msg) || !cJSON_IsNumber(del)) {
        cJSON_Delete(params);
        return {false, "", "Missing message or delay_seconds"};
    }

    int delayMs = (int)(del->valuedouble * 1000.0);
    if (delayMs < 100) delayMs = 100;
    double delaySec = del->valuedouble;  // save before cJSON_Delete

    ReminderData* rd = (ReminderData*)malloc(sizeof(ReminderData));
    if (!rd) {
        cJSON_Delete(params);
        return {false, "", "Out of memory"};
    }
    strncpy(rd->message, msg->valuestring, sizeof(rd->message) - 1);
    rd->message[sizeof(rd->message) - 1] = '\0';
    cJSON_Delete(params);

    TimerHandle_t timer = xTimerCreate(
        "reminder",
        pdMS_TO_TICKS(delayMs),
        pdFALSE,      // one-shot
        (void*)rd,
        reminder_callback
    );

    if (!timer) {
        free(rd);
        return {false, "", "Failed to create timer"};
    }

    xTimerStart(timer, 0);

    char buf[64];
    snprintf(buf, sizeof(buf), "Reminder set for %.1f seconds from now", delaySec);
    return {true, buf, ""};
}

// ---------------------------------------------------------------------------
// Built-in tool: control_gpio
// ---------------------------------------------------------------------------
ToolResult tool_control_gpio(const char* paramsJson)
{
    cJSON* params = cJSON_Parse(paramsJson);
    if (!params) return {false, "", "Invalid JSON params"};

    cJSON* pin_j   = cJSON_GetObjectItemCaseSensitive(params, "pin");
    cJSON* level_j = cJSON_GetObjectItemCaseSensitive(params, "level");

    if (!cJSON_IsNumber(pin_j) || !cJSON_IsNumber(level_j)) {
        cJSON_Delete(params);
        return {false, "", "Missing pin or level"};
    }

    int pin   = (int)pin_j->valuedouble;
    int level = (int)level_j->valuedouble;
    cJSON_Delete(params);

    gpio_config_t io_conf = {};
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    esp_err_t err = gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
    if (err != ESP_OK) {
        return {false, "", std::string("gpio_set_level failed: ") + esp_err_to_name(err)};
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "GPIO%d set to %d", pin, level ? 1 : 0);
    return {true, buf, ""};
}

// ---------------------------------------------------------------------------
// Register built-in tools
// ---------------------------------------------------------------------------
void ToolRegistry::registerBuiltinTools()
{
    registerTool({
        "get_time",
        "Returns current uptime in hours, minutes, seconds.",
        "{}",
        tool_get_time
    });

    registerTool({
        "get_system_info",
        "Returns free heap, minimum heap, uptime, core count, and chip revision.",
        "{}",
        tool_get_system_info
    });

    registerTool({
        "get_weather",
        "Fetches current weather for a city.",
        "{\"city\":\"string\"}",
        tool_get_weather
    });

    registerTool({
        "set_reminder",
        "Creates a timer that fires after delay_seconds and logs message.",
        "{\"message\":\"string\",\"delay_seconds\":\"number\"}",
        tool_set_reminder
    });

    registerTool({
        "control_gpio",
        "Sets a GPIO pin high (1) or low (0).",
        "{\"pin\":\"number\",\"level\":\"number (0 or 1)\"}",
        tool_control_gpio
    });
}
