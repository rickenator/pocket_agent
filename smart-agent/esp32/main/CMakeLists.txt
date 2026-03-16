idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi
    PRIV_REQUIRES display_driver voice_driver audio_driver ai_client wifi_manager web_server
)