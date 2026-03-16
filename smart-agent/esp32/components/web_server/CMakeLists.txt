idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server wifi_manager
    PRIV_REQUIRES espressif__cjson
)