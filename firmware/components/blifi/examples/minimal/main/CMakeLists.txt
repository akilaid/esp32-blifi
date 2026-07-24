idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    # blifi is injected automatically from main/idf_component.yml.
    REQUIRES esp_event esp_netif nvs_flash
)
