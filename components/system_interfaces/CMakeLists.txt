idf_component_register(
    SRCS "system_interfaces.c"
    INCLUDE_DIRS "." "../../main"
    REQUIRES
        sht3x
        ccs811
        trema_ph
        trema_ec
        trema_lux
        peristaltic_pump
)
