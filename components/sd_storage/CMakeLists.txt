idf_component_register(
    SRCS "sd_storage.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        fatfs
        sdmmc
        freertos
        esp_timer
)

