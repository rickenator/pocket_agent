idf_component_register(
    SRCS "ai_client.cpp" "ollama_client.cpp" "memory_manager.cpp" "goal_manager.cpp" "tool_registry.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_http_client cjson nvs_flash esp_timer
)