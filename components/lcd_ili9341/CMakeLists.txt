idf_component_register(
    SRCS "lcd_ili9341.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        driver
        esp_lcd
        esp_lcd_ili9341
        lvgl__lvgl
        lvgl_main
        encoder
        esp_timer
        main
        ph_ec_controller
)