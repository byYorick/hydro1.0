idf_component_register(
    SRCS "adaptive_pid.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        freertos
        nvs_flash
        esp_timer
        notification_system
        main
)

