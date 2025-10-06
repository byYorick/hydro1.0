idf_component_register(SRCS "automation_controller.c"
                    INCLUDE_DIRS "."
                    REQUIRES peristaltic_pump trema_relay hydro_settings esp_timer)
