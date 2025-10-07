idf_component_register(
    SRCS "mobile_app_interface.c"
    INCLUDE_DIRS
        "."
        "${IDF_PATH}/components/bt/include/esp32/include"
        "${IDF_PATH}/components/bt/host/bluedroid/api/include/api"
    PRIV_REQUIRES
        driver
        freertos
        main
        ph_ec_controller
        system_interfaces
        network_manager
        esp_http_server
        bt
        json
        nvs_flash
)

