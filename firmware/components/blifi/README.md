# blifi (ESP-IDF component)

The core ESP-IDF component and single source of truth for BLE-based Wi-Fi
provisioning: NimBLE GATT transport, X25519 + AES-256-GCM session security,
binary framing over JSON payloads, Wi-Fi scan/connect/NVS persistence, and the
provisioning state machine.

Two runnable examples: [`examples/minimal`](examples/minimal) — the smallest
integration (~20 lines, incl. reset pin + reset LED, no console) — and
[`examples/esp-idf-example`](examples/esp-idf-example), an interactive
serial-console demo. Public API is in [`include/blifi.h`](include/blifi.h):
`blifi_init` → `blifi_start`, plus `blifi_is_provisioned`, `blifi_reset_credentials`
(software reset), `blifi_get_pop`, and the hard-reset hooks below. Status/events
are delivered on the `BLIFI_EVENT` esp_event base.

## Hard reset (reset-pin factory reset)

Holding a GPIO to its configured level while the ESP32 powers on erases the stored
Wi-Fi credentials — a hardware "forget this network" that needs no serial console
or app. It uses ESP-IDF's **bootloader** factory reset, which runs before
`app_main` (Wi-Fi/BLE never come up with stale credentials) with Espressif's own
debounce. The erase is scoped to a **dedicated `blifi_nvs` partition** holding only
the Wi-Fi credentials; the default `nvs` partition — the **PoP** and your app's own
data — is left intact, so a printed QR/sticker keeps working after a reset. Design
rationale: [`docs/adr/0005-hard-reset-mechanism.md`](../../../docs/adr/0005-hard-reset-mechanism.md).

This is **opt-in** and requires two copy-paste integration steps. Without them
blifi still provisions (it logs a loud warning and falls back to the default `nvs`
partition); only the reset pin is unavailable.

### 1. Add the `blifi_nvs` partition

Merge the `blifi_nvs` row from [`partitions.example.csv`](partitions.example.csv)
into your project's `partitions.csv` (a dedicated NVS partition, ≥ 0x3000):

```
# Name,      Type, SubType, Offset,   Size
nvs,         data, nvs,     0x9000,   0x6000
phy_init,    data, phy,     0xf000,   0x1000
factory,     app,  factory, 0x10000,  0x200000
blifi_nvs,   data, nvs,     0x210000, 0x4000   # <- add this; erased by factory reset
```

### 2. Enable the bootloader factory reset

Add to your app's `sdkconfig.defaults` (adjust the GPIO to your hardware). These
are ESP-IDF *bootloader* options — they must live in the app, not the component:

```
CONFIG_BOOTLOADER_FACTORY_RESET=y
CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET=13      # GPIO held at boot to trigger
CONFIG_BOOTLOADER_FACTORY_RESET_PIN_LOW=y       # trigger on LOW (pin has a pull-up)
CONFIG_BOOTLOADER_HOLD_TIME_GPIO=3              # seconds to hold
CONFIG_BOOTLOADER_DATA_FACTORY_RESET="blifi_nvs" # erase only this partition
```

Pick a GPIO with no boot-strapping role (GPIO13 is a good ESP32 choice; avoid
GPIO0/2/4/5/12/15). To trigger: hold that pin to GND for the hold time as the
device boots.

### 3. (Optional) Erase your own app data too

The bootloader only erases partitions. To also clear your app's data (its own NVS
namespace, SPIFFS/LittleFS, …) on the boot after a reset, register a callback
**before** `blifi_init()`:

```c
static void on_data_reset(void *arg) {
    // erase your app's own storage here
}

void app_main(void) {
    blifi_register_data_reset_callback(on_data_reset, NULL);
    blifi_init(&cfg);   // detects the reset and fires the callback + BLIFI_EVENT_HARD_RESET_TRIGGERED
    // ...
}
```

`blifi_was_hard_reset()` reports whether this boot followed a hard reset (cached;
reverts to false on the next normal boot).

### 4. (Optional) Hard-reset indicator pin

Drive a GPIO active when a hard reset is detected — wire an LED, relay, or
optocoupler as a physical "credentials were wiped" signal, no app code needed.
Configure via **menuconfig → Blifi Provisioning → Hard Reset Indicator** (or
`sdkconfig.defaults`):

```
CONFIG_BLIFI_RESET_INDICATOR_ENABLE=y
CONFIG_BLIFI_RESET_INDICATOR_GPIO=2            # e.g. onboard LED
CONFIG_BLIFI_RESET_INDICATOR_ACTIVE_HIGH=y     # or ..._ACTIVE_LOW
CONFIG_BLIFI_RESET_INDICATOR_PULSE_MS=2000     # 0 = hold until re-provisioned
```

`PULSE_MS=0` holds the pin asserted until the device is re-provisioned and back
online (state machine reaches `CONNECTED`). The pin/level/pulse can also be
overridden at runtime via `blifi_config_t.reset_indicator`. **Entirely opt-in:**
when `CONFIG_BLIFI_RESET_INDICATOR_ENABLE` is off (default) the whole feature
compiles out.

> **Arduino:** the same options apply via `sdkconfig.defaults` / `partitions.csv`,
> and the `Blifi` wrapper exposes `onDataResetRequested()` / `wasHardReset()` and
> `Blifi.begin(config)`. Caveat: on the Arduino/PlatformIO (ESP-IDF 5.5) build the
> bootloader *erase* works but **app-side detection does not** (the RTC-retain flag
> is clobbered), so `wasHardReset()`, the callback/event, and this indicator only
> fire when the component is built with a standalone ESP-IDF (`idf.py`) project.
