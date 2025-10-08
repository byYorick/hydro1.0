idf_component_register(
    SRCS 
        "lvgl_ui.c"
        "ui_manager.c"
        "sensor_screens_optimized.c"
        "ph_screen.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        lvgl__lvgl 
        lcd_ili9341 
        encoder 
        main 
        ph_ec_controller
        nvs_flash
        config_manager
        data_logger
)

