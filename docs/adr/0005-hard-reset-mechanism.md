# ADR 0005 - Hard reset via reset pin

- **Status:** Accepted (2026-07-24)
- **Related:** [`../plan.md`](../plan.md) §6.1, §12, §13;
  [`../../firmware/components/blifi/README.md`](../../firmware/components/blifi/README.md)

## Context

A provisioned device needs a way back to the unprovisioned state without a serial
console - for a new owner, or to recover from credentials for a network that no
longer exists. The requirement (plan §6.1): holding a configurable GPIO while the
ESP32 powers on clears the stored Wi-Fi credentials, with an optional hook to also
clear application-defined data, and without the developer hand-rolling GPIO
polling and debounce.

## Decision

**Use ESP-IDF's built-in bootloader factory reset, not an `app_main()` GPIO poll.**
The bootloader (`CONFIG_BOOTLOADER_FACTORY_RESET`) reads a configured GPIO at
power-on and, if it is held at the configured level for a configured hold time,
erases a list of data partitions - all before `app_main`, before Wi-Fi/BLE come
up, with Espressif's own tested debounce/hold-time. This guarantees stale
credentials are gone before the state machine could ever auto-connect with them.

**Scope the erase with a dedicated `blifi_nvs` partition.** The bootloader erases
whole partitions, not NVS namespaces. So Wi-Fi credentials live in a dedicated,
small `blifi_nvs` NVS partition, and the bootloader is pointed at only that
partition (`CONFIG_BOOTLOADER_DATA_FACTORY_RESET="blifi_nvs"`). The default `nvs`
partition - PHY/RF calibration and the app's own data - is untouched.

**Keep the PoP in the default `nvs` partition (refinement of §6.1).** §6.1's prose
put "credentials and the PoP" in `blifi_nvs`. We deliberately keep only Wi-Fi
credentials there and leave the PoP in the default `nvs` partition, so a hard reset
does **not** change the PoP. A printed QR/sticker (the app's provisioning workflow)
therefore keeps working after a factory reset. (This firmware does not persist BLE
bonds - `CONFIG_BT_NIMBLE_NVS_PERSIST` is off - so nothing else needs preserving.)

**Detect the reset on the next boot for the optional app-data erase.** The
bootloader can't call app code, so "also erase my app's data" is handled one boot
later: `blifi_init()` reads the bootloader's RTC-retained factory-reset flag
(`bootloader_common_get_rtc_retain_mem_factory_reset_state()`), and if set, posts
`BLIFI_EVENT_HARD_RESET_TRIGGERED` and invokes any callback registered via
`blifi_register_data_reset_callback()`. The getter **consumes** the flag on read,
so `blifi_init` reads it exactly once and caches it; `blifi_was_hard_reset()`
returns the cache and reverts to false on the next normal boot.

**Configuration lives in the app, documented - not a component Kconfig.** The reset
GPIO, level, hold time, and erase list are ESP-IDF *bootloader* options that can
only be set in the app's `sdkconfig`/`sdkconfig.defaults`; a component Kconfig
cannot set bootloader config. The component instead ships `partitions.example.csv`
and documents the exact `sdkconfig.defaults` lines in its README. When `blifi_nvs`
is absent (integrator hasn't merged the partition), blifi logs a loud warning and
falls back to the default `nvs` partition rather than failing silently.

**Defaults for this project:** GPIO **13** (D13), reset-on-**low** (internal
pull-up; short to GND to trigger), **3-second** hold (shorter than Espressif's 5 s
default for a snappier UX; still deliberate enough to avoid accidental triggers).

## Consequences

- Credential erase is guaranteed before any auto-connect, with debounce handled by
  Espressif - no custom GPIO/timer code to maintain.
- Integrating the feature is a **manual, documented step**: merge
  `partitions.example.csv` and set the five bootloader Kconfig lines. It is opt-in;
  without the partition, provisioning still works (unscoped reset + warning).
- The PoP is stable across a hard reset - factory stickers/QRs stay valid - at the
  cost of not rotating the device secret on ownership change (acceptable; the PoP
  gates provisioning, and app-layer session crypto is per-session).
- Existing boards moving to this firmware read empty credentials once (creds moved
  partitions) and re-provision - a one-time migration, noted in the CHANGELOG.

## Alternatives considered

- **Hand-rolled GPIO poll in `app_main()`:** would run after Wi-Fi/BLE init (risking
  an auto-connect with stale creds first), and re-implement debounce/hold-time that
  the bootloader already provides. Rejected.
- **Erase the default `nvs` partition:** would also wipe PHY calibration, the PoP,
  and unrelated app data - the opposite of "only Wi-Fi credentials." Rejected in
  favour of the dedicated `blifi_nvs` partition.
- **PoP in `blifi_nvs` (literal §6.1):** a hard reset would regenerate the PoP,
  breaking any printed QR/sticker. Rejected for this project's QR-based workflow;
  see the refinement above.
