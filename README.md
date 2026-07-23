# esp32-blifi

BLE-based Wi-Fi provisioning for ESP32 — replaces the hotspot/captive-portal
flow with a Flutter app that connects over Bluetooth Low Energy, negotiates an
encrypted session, sends Wi-Fi credentials, and gets the device online.

> **Status: scaffolding only.** No firmware or app logic is implemented yet.
> This repository currently contains only the directory structure and
> placeholders. Work proceeds one phase at a time.

## What's here

| Path | Artifact | Status |
|------|----------|--------|
| `firmware/components/blifi/` | Core ESP-IDF component (single source of truth) | Not implemented |
| `arduino/Blifi/` | Thin Arduino wrapper over the component | Not implemented |
| `flutter/packages/blifi/` | Publishable Flutter/Dart package | Not implemented |
| `flutter/apps/demo_app/` | Polished demo / boilerplate app | Not implemented |

## Documentation

Architecture, protocol spec, security model, and ADRs will live under `docs/`
(`architecture.md`, `protocol-spec.md`, `security.md`, `adr/`), added in a
later phase.

## License

[MIT](LICENSE).
