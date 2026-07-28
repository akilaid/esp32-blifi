# blifi (ESP-IDF component)

The core ESP-IDF component and single source of truth for BLE-based Wi-Fi
provisioning: NimBLE GATT transport, X25519 + AES-256-GCM session security,
binary framing over JSON payloads, Wi-Fi scan/connect/NVS persistence, and the
provisioning state machine.

Three runnable examples: [`examples/very-minimal`](examples/very-minimal) - the
shortest integration (three calls, blifi manages NVS for you);
[`examples/minimal`](examples/minimal) - the smallest integration that also wires
the reset pin + reset LED (no console); and
[`examples/interactive`](examples/interactive), an interactive
serial-console demo. Public API is in [`include/blifi.h`](include/blifi.h):
`blifi_init` → `blifi_start`, plus `blifi_is_provisioned`, `blifi_reset_credentials`
(software reset), `blifi_get_pop`, and the hard-reset hooks below. Status/events
are delivered on the `BLIFI_EVENT` esp_event base.

## NVS initialisation

blifi stores the PoP in the default `nvs` partition, so that partition must be
initialised before use. By default `blifi_init()` does this for you: with
`blifi_config_t.manage_nvs = true` (the default) it calls `nvs_flash_init()`,
erasing and retrying once if the partition is full or from an incompatible
version. The application therefore does not need to call `nvs_flash_init()` at all
(see [`examples/very-minimal`](examples/very-minimal)).

Set `manage_nvs = false` if your application initialises and owns the default NVS
partition itself; blifi then assumes it is already up and never erases it (see
[`examples/minimal`](examples/minimal)). The Arduino wrapper sets this
automatically, since the Arduino core brings up NVS.

## Events

Register a handler on the `BLIFI_EVENT` esp_event base to follow provisioning on
the device (drive a status LED, gate app logic, etc.). Each post carries a
`blifi_event_data_t` (`status`, `wifi_reason`, `ip`). The events, in lifecycle
order:

| Event | When | Payload of note |
|-------|------|-----------------|
| `BLIFI_EVENT_STARTED` | `blifi_start()` completed | - |
| `BLIFI_EVENT_BLE_CONNECTED` | A phone connected over BLE | - |
| `BLIFI_EVENT_CREDENTIALS_RECEIVED` | Credentials decrypted and accepted | - |
| `BLIFI_EVENT_WIFI_CONNECTING` | Joining the AP | - |
| `BLIFI_EVENT_WIFI_CONNECTED` | Online | `ip` |
| `BLIFI_EVENT_WIFI_FAILED` | Gave up after retries; returns to provisioning | `status`, `wifi_reason` |
| `BLIFI_EVENT_PROV_TIMEOUT` | Provisioning window elapsed (opt-in, see below) | - |
| `BLIFI_EVENT_HARD_RESET_TRIGGERED` | Boot after a reset-pin factory reset | - |

This local event loop is separate from the encrypted status channel sent to the
phone; both carry the matching `blifi_status_t` code where one applies.

### Provisioning timeout (opt-in)

`blifi_config_t.prov_timeout_ms` defaults to `0` (disabled). Set it to a non-zero
value and, if no BLE central connects within that many milliseconds of
advertising, blifi posts `BLIFI_EVENT_PROV_TIMEOUT` and **stops advertising** (to
save power / reduce exposure). The window is cancelled as soon as a phone
connects. To return to provisioning afterwards, call `blifi_start()` again or
reboot.

### Stop BLE after provisioning (opt-in)

`blifi_config_t.stop_ble_after_provisioning` defaults to `false`, so BLE stays up
after provisioning (visible and connectable) exactly as before. Set it to `true`
(or `CONFIG_BLIFI_STOP_BLE_AFTER_PROVISIONING=y`) and, once provisioning succeeds
and the phone disconnects, blifi tears the whole NimBLE host down - reclaiming its
RAM and leaving nothing advertising or connectable. A short fallback timeout
covers a phone that stays connected. The device never drops the link
mid-notification, so the phone always receives the IP first.

You can also tear BLE down yourself at any time with `blifi_stop_ble()`. Either
way, the Wi-Fi give-up path re-initialises the BLE host so the device can still be
re-provisioned if it later cannot reach its network.

## Provisioning speed

A few things happen automatically to keep provisioning fast (no configuration):

- **Instant network list.** The moment the device starts advertising - before a
  phone connects, so the radio is not shared with an active BLE link - it scans
  for Wi-Fi networks and caches the result. When the app asks for the list it is
  served from that cache instead of waiting for a fresh scan. The cache is
  RAM-only (never NVS), holds only public SSID/RSSI/channel data (never
  credentials), and is cleared after a successful connection.
- **Fast connect.** Joining the AP uses a fast scan (first matching AP) rather
  than scanning every channel. No channel is pinned, so the device still connects
  if the router later changes channel.

## Proof-of-Possession (PoP)

The PoP is the short code the phone must present to complete provisioning (it is
mixed into the session key schedule, so a wrong PoP simply cannot derive the keys).
By default blifi generates a random one on first boot, stores it in the default
`nvs` partition (so it survives a reset-pin factory reset), and logs it once over
serial. `blifi_get_pop()` returns it. A PoP is 8 uppercase Crockford base32
characters (`0-9` and `A-Z` excluding `I, L, O, U`), e.g. `ABCD2345`.

Two knobs on `blifi_config_t`:

- `require_pop` (default `true`) - set `false` only for development; provisioning is
  then merely passively secure (an active MITM can succeed). Never ship it off.
- `fixed_pop` - pin the PoP to an exact value instead of auto-generating it.

### Setting a fixed PoP

Use this when you print/QR the code yourself, or want a deterministic value per
unit. Two ways, both validated against the 8-char Crockford format:

- **Build time (recommended, fails the build on a bad value):** set
  `CONFIG_BLIFI_FIXED_POP` via menuconfig (**Blifi Provisioning -> Fixed
  Proof-of-Possession**) or in `sdkconfig.defaults`:
  ```
  CONFIG_BLIFI_FIXED_POP="ABCD2345"
  ```
  A wrong length or an invalid character stops the build with a clear error.
- **Runtime:** set the field before `blifi_init()`:
  ```c
  blifi_config_t cfg = BLIFI_DEFAULT_CONFIG();
  cfg.fixed_pop = "ABCD2345";   // validated here; blifi_init() fails if malformed
  blifi_init(&cfg);
  ```

A fixed PoP overrides any NVS-stored value and is not persisted. Leave both unset
(the default) to keep the auto-generated, NVS-backed PoP.

> **Security note:** the format is validated, not the secrecy. Reusing one PoP
> across many devices means anyone who learns it can provision all of them - prefer
> a value that is unique per unit.

## Hard reset (reset-pin factory reset)

Holding a GPIO to its configured level while the ESP32 powers on erases the stored
Wi-Fi credentials - a hardware "forget this network" that needs no serial console
or app. It uses ESP-IDF's **bootloader** factory reset, which runs before
`app_main` (Wi-Fi/BLE never come up with stale credentials) with Espressif's own
debounce. The erase is scoped to a **dedicated `blifi_nvs` partition** holding only
the Wi-Fi credentials; the default `nvs` partition - the **PoP** and your app's own
data - is left intact, so a printed QR/sticker keeps working after a reset. Design
rationale: [ADR 0005: hard-reset mechanism](https://github.com/akilaid/esp32-blifi/blob/main/docs/adr/0005-hard-reset-mechanism.md).

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
are ESP-IDF *bootloader* options - they must live in the app, not the component:

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

Drive a GPIO active when a hard reset is detected - wire an LED, relay, or
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
