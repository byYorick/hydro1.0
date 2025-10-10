idf_component_register(
    SRCS "app_main.c" "montserrat14_ru.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        freertos
        esp_timer
        nvs_flash
        spi_flash
        lvgl__lvgl
        lvgl_ui
        system_tasks
        encoder
        peristaltic_pump
        lcd_ili9341
        trema_relay
        config_manager
        sht3x
        i2c_bus
        error_handler
        ccs811
        trema_ph
        trema_ec
        trema_lux
        notification_system
        data_logger
        task_scheduler
        system_interfaces
        ph_ec_controller
        # IoT компоненты временно отключены для компиляции PID
        # network_manager
        # mqtt_client
        # telegram_bot
        # sd_storage
        # ai_controller
        # mesh_network
        esp_netif
        esp_event
)
