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

