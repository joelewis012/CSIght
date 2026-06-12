idf_component_register(
    SRCS "csi_handler.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        freertos
        esp_system
)
