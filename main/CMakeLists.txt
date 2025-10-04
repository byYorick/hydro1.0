idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS ""
    PRIV_REQUIRES
        driver
        freertos
        nvs_flash
        lvgl
        i2c_bus
        sht3x
        ccs811
        trema_ph
        trema_ec
        trema_lux
        lvgl_main
        encoder
        peristaltic_pump
        lcd_ili9341
        trema_relay
        sensor_screens
)