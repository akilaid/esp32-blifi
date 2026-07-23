# Changelog — blifi (ESP-IDF component)

All notable changes to the firmware component are documented here. Format based
on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this component is
versioned independently under [Semantic Versioning](https://semver.org/).

## [Unreleased]

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

### Fixed
- NimBLE host-task stack overflow when handling a scan request: moved the
  encrypted-record buffer off the stack (it is lock-guarded) and raised
  `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE`. Found via live phone interop.

