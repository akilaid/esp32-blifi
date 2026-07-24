# Blifi — Arduino-style Wi-Fi provisioning for ESP32

Provision an ESP32's Wi-Fi credentials over an encrypted **BLE** session (no
hotspot or captive portal), from a ~10-line sketch. `Blifi` is a thin C++ wrapper
over the [`blifi`](../../firmware/components/blifi) ESP-IDF component — it turns
the component's events into friendly Arduino callbacks. No provisioning/crypto/BLE
logic is duplicated here.

```cpp
#include "Blifi.h"

void setup() {
  Serial.begin(115200);
  Blifi.onProvisioned([](IPAddress ip) {
    Serial.print("Online! IP: ");
    Serial.println(ip);
  });
  Blifi.begin();
  Serial.print("PoP: ");
  Serial.println(Blifi.pop());   // show this to whoever provisions the device
}
void loop() {}
```

## Why PlatformIO (and not the plain Arduino IDE)?

The `blifi` component uses **NimBLE**, but the Arduino IDE's stock ESP32 core is
built with **Bluedroid** and ships a *precompiled* ESP-IDF — so it can neither
build an external IDF component nor switch the BLE stack. This project therefore
builds through **PlatformIO** with `framework = arduino, espidf`, which compiles
ESP-IDF (and the `blifi` component) from source with NimBLE enabled. You still
write ordinary Arduino code (`setup()`/`loop()`, `Serial`, `IPAddress`).

## Build & upload

1. Install **VS Code** + the **PlatformIO** extension.
2. Open this folder (`arduino/Blifi`) as a PlatformIO project.
3. Click **Upload** (or `pio run -t upload`). The first build downloads the
   toolchain and compiles ESP-IDF from source — it takes a few minutes; later
   builds are fast.
4. Open the **Serial Monitor** at 115200. It prints the Proof-of-Possession.
5. Provision with the blifi phone app (connect → enter the PoP → pick your
   network). The monitor prints `Online! IP: …`.

The `blifi` component is pulled automatically from the monorepo via
`src/idf_component.yml` (a relative path — no symlinks, works on any OS).

## API

| Call | Purpose |
|------|---------|
| `Blifi.begin()` / `Blifi.begin("my-name")` | Initialise + start provisioning (optional BLE name). |
| `Blifi.onProvisioned(cb)` | `cb(IPAddress ip)` when the device comes online. |
| `Blifi.onStatusChanged(cb)` | `cb(blifi_status_t)` on every status change. |
| `Blifi.isProvisioned()` | Whether Wi-Fi credentials are stored. |
| `Blifi.resetCredentials()` | Forget Wi-Fi and re-enter provisioning (software reset). |
| `Blifi.pop()` | The device's Proof-of-Possession string. |
| `Blifi.statusString(s)` | Human-readable name for a status code. |
| `Blifi.onDataResetRequested(cb)` / `Blifi.wasHardReset()` | Reset-pin hard-reset hooks — see below. |

## Reset-pin hard reset (advanced, off by default)

Holding a GPIO at power-on to factory-reset the device is a **bootloader**
feature. It works in this PlatformIO build but requires extra config
(`CONFIG_BOOTLOADER_FACTORY_RESET`, a dedicated `blifi_nvs` partition) — see the
component's
[hard-reset guide](../../firmware/components/blifi/README.md#hard-reset-reset-pin-factory-reset).
It is **not** enabled in this basic setup, so `wasHardReset()` returns false and
`onDataResetRequested()` does not fire; the software `resetCredentials()` is the
everyday "forget Wi-Fi" path.

## Notes

- Requires the **pioarduino** platform (arduino-esp32 3.x / ESP-IDF 5.5), pinned
  in `platformio.ini`. The blifi component (written for ESP-IDF 6.0) builds
  unchanged on 5.5.
- Board defaults to `esp32dev` (classic ESP32); adjust `board` in
  `platformio.ini` for other modules.
