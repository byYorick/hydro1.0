idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos
        esp_timer
        main
    PRIV_REQUIRES 
        sht3x
        ccs811
        trema_ph
        trema_ec
        trema_lux
)

