# blifi interactive example

The fullest blifi example: BLE Wi-Fi provisioning plus an interactive serial
console for poking at the device at runtime. It also wires the bootloader
**reset pin** (GPIO13) and manages the default NVS partition itself
(`cfg.manage_nvs = false`).

For the shortest integration (three calls, blifi manages NVS) see
[`very-minimal`](../very-minimal); for the smallest one that adds the reset pin
and reset LED without a console see [`minimal`](../minimal).

## Build & flash

```bash
get_idf                     # activate ESP-IDF (see repo setup)
cd firmware/components/blifi/examples/interactive
idf.py set-target esp32
idf.py -p <PORT> flash monitor      # e.g. /dev/cu.usbserial-240
```

On first boot the log prints the device name (`blifi-XXXX`) and its
Proof-of-Possession. Provision with the companion app (the
[`blifi` Flutter package](https://pub.dev/packages/blifi) or the
[demo app](https://github.com/akilaid/esp32-blifi/tree/main/flutter/apps/demo_app)):
connect, enter the PoP, pick a network.

## Console commands

At the `blifi>` prompt (type `help` for the list):

| Command | Description |
|---------|-------------|
| `status` | Show provisioning + Wi-Fi status and IP |
| `pop` | Show the device's Proof-of-Possession |
| `reset` | Erase stored credentials and return to provisioning |
| `selftest` | Run the crypto / framing self-tests |
| `hardreset?` | Report whether this boot followed a reset-pin hard reset |

Status transitions are also logged as they happen (`[event] WIFI_CONNECTING`,
`WIFI_CONNECTED ip=…`, etc.).

## Factory reset

Hold the reset pin (GPIO13) low while the device boots (press the button, tap
EN/RST, keep holding ~3 s). The bootloader erases the dedicated `blifi_nvs`
partition - Wi-Fi credentials only; the PoP survives, so a printed QR/sticker
keeps working - and the device is back in provisioning mode. The app's
data-reset callback fires on that boot. You can also confirm it afterward with
the `hardreset?` command.
