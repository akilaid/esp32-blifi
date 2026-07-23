# blifi — BLE-based Wi-Fi provisioning for ESP32.
# Phase 2: Wi-Fi core only (scan/connect/persistence/retry). BLE transport,
# security, and the provisioning manager are added in later phases.
idf_component_register(
    SRCS
        "src/wifi_manager/wifi_manager.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
    PRIV_REQUIRES
        nvs_flash
        esp_timer
)
