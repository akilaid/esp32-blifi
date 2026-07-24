# blifi — BLE-based Wi-Fi provisioning for ESP32.
# Wi-Fi core + protocol framing + PSA session crypto + NimBLE transport +
# provisioning state machine.
idf_component_register(
    SRCS
        "src/wifi_manager/wifi_manager.c"
        "src/protocol/frame.c"
        "src/protocol/messages.c"
        "src/security/crypto.c"
        "src/security/pop.c"
        "src/ble_transport/ble_transport.c"
        "src/provisioning_manager/provisioning_manager.c"
        "src/hard_reset/hard_reset.c"
        "src/blifi_selftest.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
        "src/protocol"
        "src/security"
        "src/ble_transport"
        "src/provisioning_manager"
        "src/hard_reset"
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
    PRIV_REQUIRES
        nvs_flash
        esp_timer
        esp_hw_support
        esp_app_format
        bootloader_support
        esp_driver_gpio
        mbedtls
        bt
)
