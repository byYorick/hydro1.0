idf_component_register(
    SRCS "mesh_network.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_wifi
        esp_event
        freertos
        esp_timer
)

