idf_component_register(
    SRCS "telegram_bot.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_http_client
        json
        freertos
        main
)

