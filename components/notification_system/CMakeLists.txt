idf_component_register(SRCS "notification_system.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos
                    PRIV_REQUIRES data_logger config_manager nvs_flash)

