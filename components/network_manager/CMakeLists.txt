idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        esp_https_ota
        lwip
        nvs_flash
    PRIV_REQUIRES
        esp_timer
        esp_eth
        esp_http_client
        esp_https_server
        app_update
)
