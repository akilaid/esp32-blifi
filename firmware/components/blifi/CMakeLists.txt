# blifi - BLE-based Wi-Fi provisioning for ESP32.
# Wi-Fi core + protocol framing + PSA session crypto + NimBLE transport +
# provisioning state machine.

# Build-time validation of a user-supplied fixed PoP (CONFIG_BLIFI_FIXED_POP).
# Empty (the default) means auto-generate at runtime, so there is nothing to
# check. A malformed value fails the build here rather than shipping a broken
# device. The runtime path (blifi_config_t.fixed_pop) enforces the same rule via
# blifi_pop_validate().
if(DEFINED CONFIG_BLIFI_FIXED_POP AND NOT "${CONFIG_BLIFI_FIXED_POP}" STREQUAL "")
    string(LENGTH "${CONFIG_BLIFI_FIXED_POP}" _blifi_pop_len)
    if(NOT _blifi_pop_len EQUAL 8)
        message(FATAL_ERROR
            "CONFIG_BLIFI_FIXED_POP must be exactly 8 characters "
            "(got ${_blifi_pop_len}: \"${CONFIG_BLIFI_FIXED_POP}\").")
    endif()
    if(NOT "${CONFIG_BLIFI_FIXED_POP}" MATCHES "^[0-9A-HJKMNP-TV-Z]+$")
        message(FATAL_ERROR
            "CONFIG_BLIFI_FIXED_POP must use only uppercase Crockford base32 "
            "characters (0-9 and A-Z excluding I, L, O, U): "
            "\"${CONFIG_BLIFI_FIXED_POP}\".")
    endif()
endif()

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
