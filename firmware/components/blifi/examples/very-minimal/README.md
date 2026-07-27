# blifi very-minimal example

The shortest possible blifi integration: BLE Wi-Fi provisioning and nothing
else. `app_main()` is three calls - build the default config, `blifi_init()`,
`blifi_start()` - with **no `nvs_flash_init()`**. `blifi_init()` initialises the
default NVS partition itself (`cfg.manage_nvs` defaults to `true`; the PoP is
stored there).

If your application manages NVS itself, set `cfg.manage_nvs = false` and call
`nvs_flash_init()` before `blifi_init()`, as shown in the sibling
[`minimal`](../minimal) example. For an interactive demo with serial-console
commands (`status`, `pop`, `reset`, `selftest`) plus a hardware reset pin and
reset LED, see [`interactive`](../interactive).

> **This is a getting-started sketch, not a production template.** Two things to
> revisit before shipping:
> - **NVS ownership.** For convenience `blifi_init()` initialises the default NVS
>   partition here (and erases + retries once if it is corrupt). If your firmware
>   stores its own data in the default `nvs` partition, prefer owning NVS yourself
>   (`cfg.manage_nvs = false` + your own `nvs_flash_init()`) so you control that
>   erase - see [`minimal`](../minimal).
> - **No field reset.** There is no hardware reset pin here, so re-provisioning in
>   the field means calling `blifi_reset_credentials()` from your own firmware or
>   reflashing. Production devices usually want the bootloader factory-reset pin
>   from [`minimal`](../minimal) / [`interactive`](../interactive).

## Build and run

```bash
idf.py set-target esp32        # or esp32s3 / esp32c3
idf.py build flash monitor
```

On first boot the log prints the device name (`blifi-XXXX`) and its
Proof-of-Possession. Provision with the companion app (the
[`blifi` Flutter package](https://pub.dev/packages/blifi) or the
[demo app](https://github.com/akilaid/esp32-blifi/tree/main/flutter/apps/demo_app)):
connect, enter the PoP, pick a network. The device then reconnects by itself on
every boot.

This example has no reset pin or reset LED; to return to provisioning, erase the
credentials from firmware (`blifi_reset_credentials()`) or reflash. The
[`minimal`](../minimal) and [`interactive`](../interactive) examples add
the bootloader factory-reset pin and the reset indicator LED.
