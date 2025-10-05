idf_component_register(
    SRCS "system_tasks.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        esp_system
        esp_timer
        sht3x
        ccs811
        trema_ph
        trema_ec
        trema_lux
        notification_system
        data_logger
        task_scheduler
        ph_ec_controller
        config_manager
        lvgl_main
        encoder
)

