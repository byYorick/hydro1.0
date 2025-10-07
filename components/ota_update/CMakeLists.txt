idf_component_register(
    SRCS ""  # Empty for now, just header file
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        esp_http_client
        esp_https_ota
)

