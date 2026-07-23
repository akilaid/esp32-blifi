idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES blifi console esp_wifi esp_netif nvs_flash
)
