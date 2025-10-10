idf_component_register(
    SRCS "pump_pid_manager.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        pid_controller
        pump_manager
        config_manager
        main
        freertos
        esp_timer
)

