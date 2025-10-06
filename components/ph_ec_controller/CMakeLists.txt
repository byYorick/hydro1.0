idf_component_register(SRCS "ph_ec_controller.c"
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES freertos peristaltic_pump main system_interfaces)

