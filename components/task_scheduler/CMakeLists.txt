idf_component_register(
    SRCS "task_scheduler.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        freertos
        nvs_flash
        esp_timer
)
