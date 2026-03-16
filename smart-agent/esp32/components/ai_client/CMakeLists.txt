idf_component_register(
    SRCS "ai_client.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_http_client cjson
)