idf_component_register(SRCS "encoder.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver i2c_bus esp_timer)