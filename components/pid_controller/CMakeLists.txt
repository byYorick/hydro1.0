idf_component_register(
    SRCS "pid_controller.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_timer
)

