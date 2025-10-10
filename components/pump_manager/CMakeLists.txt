idf_component_register(
    SRCS "pump_manager.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        peristaltic_pump
        config_manager
        main
        freertos
        esp_timer
        driver
)

