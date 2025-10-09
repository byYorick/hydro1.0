idf_component_register(
    SRCS "error_handler.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        freertos
        lvgl
        notification_system
)

