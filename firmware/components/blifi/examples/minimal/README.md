# blifi minimal example

The smallest useful blifi integration: BLE Wi-Fi provisioning plus the two
hardware niceties - a **reset pin** and a **reset LED** - with no console/CLI.
`app_main()` is ~20 lines; the reset pin and LED are pure configuration
(`sdkconfig.defaults` + `partitions.csv`), not code.

For an interactive demo with serial-console commands (`status`, `pop`, `reset`,
`selftest`), see the sibling
[`esp-idf-example`](../esp-idf-example).

## Wiring (optional)

| GPIO | Role | Notes |
|------|------|-------|
| 13 | Reset pin | Momentary button to GND; hold ~3 s during boot/reset |
| 2 | Reset LED | Onboard LED on many ESP32 devkits - no wiring needed there |

Both pins are configurable in `sdkconfig.defaults` (`menuconfig` →
Bootloader config for the pin, `Blifi Provisioning → Hard Reset Indicator`
for the LED).

## Build and run

```bash
idf.py set-target esp32        # or esp32s3 / esp32c3
idf.py build flash monitor
```

On first boot the log prints the device name (`blifi-XXXX`) and its
Proof-of-Possession. Provision with the companion app (the
[`blifi` Flutter package](https://pub.dev/packages/blifi) or the
[demo app](https://github.com/akilaid/esp32-blifi/tree/main/flutter/apps/demo_app)):
connect, enter the PoP, pick a network. The device then reconnects by itself
on every boot.

## Factory reset

Hold the reset pin (GPIO13) low while the device boots (press the button, tap
EN/RST, keep holding ~3 s). The bootloader erases the dedicated `blifi_nvs`
partition - Wi-Fi credentials only; the PoP survives, so a printed QR/sticker
keeps working - and on that boot the LED (GPIO2) pulses for 2 s and the app's
data-reset callback fires. The device is back in provisioning mode.
