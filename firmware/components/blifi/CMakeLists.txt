# blifi — BLE-based Wi-Fi provisioning for ESP32.
# Phase 3 (Stage A): Wi-Fi core + protocol framing + session crypto + self-tests.
# ble_transport and provisioning_manager are added in Stage B.
idf_component_register(
    SRCS
        "src/wifi_manager/wifi_manager.c"
        "src/protocol/frame.c"
        "src/protocol/messages.c"
        "src/security/crypto.c"
        "src/security/pop.c"
        "src/blifi_selftest.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
        "src/protocol"
        "src/security"
        "src/ble_transport"
        "src/provisioning_manager"
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
    PRIV_REQUIRES
        nvs_flash
        esp_timer
        esp_hw_support
        mbedtls
)
