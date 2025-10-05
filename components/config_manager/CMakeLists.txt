idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "."
    REQUIRES main ph_ec_controller
    PRIV_REQUIRES
        nvs_flash
)
