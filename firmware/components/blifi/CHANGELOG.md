# Changelog — blifi (ESP-IDF component)

All notable changes to the firmware component are documented here. Format based
on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this component is
versioned independently under [Semantic Versioning](https://semver.org/).

## [Unreleased]

Nothing yet.

## [0.1.1] — 2026-07-25

### Added
- Minimal example (`examples/minimal`): the smallest useful integration —
  provisioning plus the reset pin and reset-indicator LED via pure
  configuration, without the serial-console commands of `esp-idf-example`.

## [0.1.0] — 2026-07-24

First public release on the ESP Component Registry (`akilaid/blifi`).

### Added
- Wi-Fi station manager (`blifi_wifi_manager`): active scan, connect using
  supplied or NVS-stored credentials, credential persistence, and a
  retry/backoff connection state machine with a per-attempt connect timeout and
  fast-fail on auth errors.
- Public headers `blifi.h`, `blifi_status.h`, `blifi_wifi_manager.h`; the
  `BLIFI_EVENT` esp_event base (Wi-Fi events live; BLE/provisioning events
  declared for later phases); status/error codes mirroring `docs/protocol-spec.md`.
- Component manifest (`idf_component.yml`) and build (`CMakeLists.txt`).
- Serial-console example (`examples/esp-idf-example`) exercising the manager
  with no BLE.
- Wire protocol: 8-byte big-endian framing with chunking/reassembly and a
  self-contained JSON payload codec (no external cJSON dependency).
- Session security over the PSA Crypto API (mbedTLS 4.x / TF-PSA-Crypto):
  X25519 key agreement, HKDF-SHA256 key schedule with PoP mixing, HMAC-SHA256
  key confirmation, and AES-256-GCM records with per-direction counters and
  replay protection. Proof-of-Possession generation (8-char Crockford base32).
- NimBLE GATT transport (`ble_transport`): the provisioning service with its five
  characteristics, advertising as `blifi-XXXX`, MTU-aware chunked notifications.
- Provisioning manager (`provisioning_manager`): state machine tying BLE ↔
  handshake ↔ Wi-Fi, PoP lifecycle + confirmation lockout, encrypted status
  notifications, and the public API (`blifi_init/start/stop/is_provisioned/
  reset_credentials/get_pop`).
- On-device self-tests (`blifi_selftest`, `selftest` command) with RFC 7748 /
  RFC 5869 / AES-GCM known-answer vectors plus session and framing round-trips.
- Hard reset via reset pin (docs/plan.md §6.1, ADR 0005): the ESP-IDF bootloader
  factory reset erases a dedicated `blifi_nvs` partition (Wi-Fi credentials only)
  when a GPIO is held at boot. New public API `blifi_register_data_reset_callback()`
  and `blifi_was_hard_reset()`, plus the `BLIFI_EVENT_HARD_RESET_TRIGGERED` event,
  let the app erase its own data on the next boot. Ships `partitions.example.csv`;
  the reset GPIO/level/hold-time and erase target are set via bootloader Kconfig in
  the app's `sdkconfig.defaults` (see README). The PoP stays in the default `nvs`
  partition, so it survives a hard reset (printed QR/sticker keeps working).

- Optional hard-reset indicator pin (docs/plan.md §6.2): a `Kconfig.projbuild`
  "Blifi Provisioning → Hard Reset Indicator" menu
  (`CONFIG_BLIFI_RESET_INDICATOR_ENABLE`/`_GPIO`/`_ACTIVE_LEVEL`/`_PULSE_MS`) drives
  a GPIO active when a hard reset is detected, driven from the `hard_reset` module.
  `PULSE_MS=0` holds until re-provisioned (`CONNECTED`). Pin/level/pulse are also
  runtime-overridable via `blifi_config_t.reset_indicator`. Fully opt-in — the whole
  feature compiles out when disabled (default).

### Changed
- Wi-Fi credentials now persist in the dedicated `blifi_nvs` partition (via
  `blifi_wifi_manager_config_t.nvs_partition`) instead of the default `nvs`
  partition, scoping the hard reset to credentials. When `blifi_nvs` is absent,
  blifi logs a loud warning and falls back to the default `nvs` partition.
  **Migration:** existing boards read empty on first boot of this firmware and
  re-provision once.

### Fixed
- NimBLE host-task stack overflow when handling a scan request: moved the
  encrypted-record buffer off the stack (it is lock-guarded) and raised
  `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE`. Found via live phone interop.

