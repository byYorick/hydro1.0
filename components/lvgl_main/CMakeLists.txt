idf_component_register(SRCS "lvgl_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl__lvgl lcd_ili9341 encoder sensor_screens main ph_ec_controller)