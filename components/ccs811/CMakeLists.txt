idf_component_register(SRCS "ccs811.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver i2c_bus)