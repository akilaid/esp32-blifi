idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    # blifi is injected automatically from main/idf_component.yml. The app itself
    # does not touch NVS (blifi_init manages it), so nvs_flash is not required here.
    REQUIRES esp_event esp_netif
)
