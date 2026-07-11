idf_component_register(
    SRCS "mesh_node.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        freertos
        esp_system
        esp_timer
        nvs_flash
)
