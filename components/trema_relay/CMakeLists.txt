idf_component_register(SRCS "trema_relay.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver i2c_bus)