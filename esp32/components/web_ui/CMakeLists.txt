idf_component_register(
    SRCS "web_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_http_server
        esp_event
        freertos
        json
        app_update
    PRIV_REQUIRES
        csi_handler
)
