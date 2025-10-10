idf_component_register(
    SRCS "pump_manager.c"
    INCLUDE_DIRS "."
    REQUIRES 
        peristaltic_pump 
        freertos 
        config_manager 
        data_logger 
        notification_system
        esp_timer
)

