idf_component_register(SRCS "sht3x.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver i2c_bus)