idf_component_register(
    SRCS "sensor_screens.c" "ph_screen.c" "ec_screen.c" "temp_screen.c" "humidity_screen.c" "lux_screen.c" "co2_screen.c"
    INCLUDE_DIRS "."
    REQUIRES "lvgl" "lvgl_main"
)
