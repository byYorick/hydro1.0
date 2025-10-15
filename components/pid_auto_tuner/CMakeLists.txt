idf_component_register(
    SRCS "pid_auto_tuner.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        freertos
        esp_timer
        pump_manager
        notification_system
        config_manager
)

