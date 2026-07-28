# Changelog - blifi (ESP-IDF component)

All notable changes to the firmware component are documented here. Format based
on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this component is
versioned independently under [Semantic Versioning](https://semver.org/).

## [Unreleased]

Nothing yet.

## [0.4.0] - 2026-07-28

### Added
- Opt-in `blifi_config_t.stop_ble_after_provisioning` (default `false`) and the
  `CONFIG_BLIFI_STOP_BLE_AFTER_PROVISIONING` Kconfig default. When enabled, the
  BLE stack is fully torn down (NimBLE host deinitialised, RAM reclaimed, nothing
  advertising or connectable) once provisioning succeeds and the phone
  disconnects, with a short fallback timeout if it lingers. The device never
  drops the link mid-notification, so the IP is always delivered first. The Wi-Fi
  give-up path still brings BLE back for re-provisioning.
- Public `esp_err_t blifi_stop_ble(void)` - tear the BLE stack down on demand
  (deferred onto a timer task, so it is safe to call from any context, including a
  blifi callback). A later `blifi_start()` re-initialises the host.

### Notes
- Default behaviour is unchanged: with the flag off, BLE stays up after
  provisioning exactly as before. Verified on ESP32-S3 hardware, including the
  full-teardown then re-init (re-provision) cycle.

### Fixed
- Examples now require the component version they actually build against. All
  three examples use `cfg.manage_nvs` (added in 0.3.0), but their
  `main/idf_component.yml` pinned `>=0.1.0`; a registry extract could resolve a
  pre-0.3.0 component that fails to compile. Raised the floor to `>=0.3.0`.
  (Monorepo/CI builds were unaffected, since they build via `override_path`.)

### Changed
- Refreshed stale documentation comments: the `blifi.h` file header no longer
  describes a "Phase 2, Wi-Fi only" state that no longer exists, and the leftover
  `(Phase 3)` markers and internal `docs/plan.md` references were removed from the
  public headers, Kconfig help, and source comments. No code or API change.

## [0.3.1] - 2026-07-27

### Added
- The `BLIFI_EVENT` loop now posts the full provisioning lifecycle:
  `BLIFI_EVENT_STARTED`, `BLIFI_EVENT_BLE_CONNECTED`, and
  `BLIFI_EVENT_CREDENTIALS_RECEIVED` were declared but never fired; they are now
  emitted at their call sites, so an on-device consumer (e.g. a status LED) can
  follow the handshake, not just the Wi-Fi phase.
- `blifi_config_t.prov_timeout_ms` is now enforced (it was previously ignored).
  When set to a non-zero value, blifi stops advertising and posts
  `BLIFI_EVENT_PROV_TIMEOUT` if no BLE central connects within the window. Opt-in;
  default `0` keeps the previous "no timeout" behaviour.

### Removed
- `BLIFI_EVENT_REPROVISIONING_TRIGGERED` (a local `esp_event` id). It was posted
  immediately after `BLIFI_EVENT_WIFI_FAILED` with an identical payload; its one
  internal action (return to provisioning) now happens inside the `WIFI_FAILED`
  handler. Subscribe to `BLIFI_EVENT_WIFI_FAILED` instead. No wire-protocol change
  (these ids are local); no consumer in this repo referenced the removed event.

### Fixed
- After Wi-Fi is given up on, the device now ensures BLE advertising is running and
  waits for new credentials, instead of immediately re-attempting the same stored
  credentials. Previously the give-up path re-armed Wi-Fi, which retried the
  unchanged (often wrong) credentials in a tight loop; a boot that was already
  provisioned but could not connect could also be left unreachable for
  re-provisioning. Credentials are unchanged until the app submits new ones.

## [0.3.0] - 2026-07-27

### Added
- `blifi_config_t.manage_nvs` (default `true`): `blifi_init()` now initialises the
  default NVS partition itself (erasing and retrying once on
  `ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND`), so applications no
  longer need to call `nvs_flash_init()`. Set `manage_nvs = false` to keep managing
  NVS in the application.
- New [`examples/very-minimal`](examples/very-minimal): the shortest integration -
  three calls in `app_main()`, no `nvs_flash_init()`.

### Changed
- The `minimal` and `interactive` examples now set `manage_nvs = false` and keep
  their own `nvs_flash_init()`, to demonstrate the opt-out.
- Renamed the `esp-idf-example` example to `interactive`.

## [0.2.0] - 2026-07-25

### Added
- Fixed (user-defined) Proof-of-Possession. Set an exact 8-character Crockford
  base32 PoP instead of auto-generating one, two ways:
  - `CONFIG_BLIFI_FIXED_POP` (menuconfig / `sdkconfig.defaults`), **validated at
    build time** - a wrong length or an invalid character fails the build.
  - `blifi_config_t.fixed_pop` at runtime, validated in `blifi_init()` (returns an
    error on a malformed value).
  A fixed PoP overrides any NVS-stored value and is not persisted; leaving both
  unset keeps the auto-generated, NVS-backed PoP (unchanged default).
- Public `blifi_pop_validate()` and a self-test covering the format rules.

## [0.1.3] - 2026-07-25

### Changed
- Example manifests: removed the non-standard `description` field that 0.1.2 had
  added to each example's `main/idf_component.yml` (no well-maintained component
  ships one) and pinned the back-dependency `akilaid/blifi: "^0.1"` instead of
  `"*"`, matching espressif/led_strip and esp-iot-solution/button.

### Notes
- The registry auto-generates (and badges "Generated by LLM") the short example
  descriptions from each example's README/code; there is no author override via
  the manifest. The example READMEs are the place to shape that text.
- Publishing is slow server-side: blifi's NimBLE + mbedTLS/PSA dependency graph
  makes the registry's "Processing examples" step take minutes, so the upload
  action can report a timeout even though the version does publish (0.1.2 and
  0.1.3 both landed after "failed" runs). CI now confirms the outcome against the
  registry API instead of trusting the action's exit code.

## [0.1.2] - 2026-07-25

### Changed
- Docs only: removed em-dashes throughout the README, headers, and code comments
  (hyphens/colons instead). No code or API changes.
- Both example manifests now carry an author-written `description` field, so the
  ESP Component Registry shows those instead of an auto-generated summary.
- README links the hard-reset ADR by absolute GitHub URL; the old relative path
  pointed outside the packaged component and broke on the registry.

## [0.1.1] - 2026-07-25

### Added
- Minimal example (`examples/minimal`): the smallest useful integration -
  provisioning plus the reset pin and reset-indicator LED via pure
  configuration, without the serial-console commands of `esp-idf-example`.

## [0.1.0] - 2026-07-24

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
- Hard reset via reset pin (ADR 0005): the ESP-IDF bootloader
  factory reset erases a dedicated `blifi_nvs` partition (Wi-Fi credentials only)
  when a GPIO is held at boot. New public API `blifi_register_data_reset_callback()`
  and `blifi_was_hard_reset()`, plus the `BLIFI_EVENT_HARD_RESET_TRIGGERED` event,
  let the app erase its own data on the next boot. Ships `partitions.example.csv`;
  the reset GPIO/level/hold-time and erase target are set via bootloader Kconfig in
  the app's `sdkconfig.defaults` (see README). The PoP stays in the default `nvs`
  partition, so it survives a hard reset (printed QR/sticker keeps working).

- Optional hard-reset indicator pin: a `Kconfig.projbuild`
  "Blifi Provisioning → Hard Reset Indicator" menu
  (`CONFIG_BLIFI_RESET_INDICATOR_ENABLE`/`_GPIO`/`_ACTIVE_LEVEL`/`_PULSE_MS`) drives
  a GPIO active when a hard reset is detected, driven from the `hard_reset` module.
  `PULSE_MS=0` holds until re-provisioned (`CONNECTED`). Pin/level/pulse are also
  runtime-overridable via `blifi_config_t.reset_indicator`. Fully opt-in - the whole
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

