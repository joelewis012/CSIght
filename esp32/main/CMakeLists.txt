idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        nvs_flash
        driver
        esp_system
        freertos
        esp_http_server
        json
    PRIV_REQUIRES
        csi_handler
        web_ui
        mesh_node
)
