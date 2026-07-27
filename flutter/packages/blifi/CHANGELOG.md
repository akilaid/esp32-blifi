# Changelog - blifi (Flutter package)

All notable changes to the Flutter package are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this package is
versioned independently under [Semantic Versioning](https://semver.org/).

## 0.3.1 - 2026-07-27

Version alignment: bumped to 0.3.1 alongside the ESP-IDF component and Arduino
library so the monorepo's published artifacts share one version. No functional or
API changes in the Dart package (the component's 0.3.1 change - completing the
device-side `BLIFI_EVENT` lifecycle - is device-side only; the wire protocol and
status bytes are unchanged).

## 0.3.0 - 2026-07-27

Version alignment: bumped to 0.3.0 alongside the ESP-IDF component and Arduino
library so the monorepo's published artifacts share one version. No functional or
API changes in the Dart package (the component's 0.3.0 change - blifi_init now
manages NVS by default - is device-side only).

## 0.2.0 - 2026-07-25

Version alignment: bumped to 0.2.0 alongside the ESP-IDF component and Arduino
library so the monorepo's published artifacts share one version. No functional or
API changes in the Dart package (the fixed-PoP feature this release adds is
device-side; the app just sends whatever PoP the user enters, as before).

## 0.1.3 - 2026-07-25

Docs only: removed em-dashes from the README, the pubspec description, and code
comments. No functional or API changes.

## 0.1.1 - 2026-07-25

No functional changes; verifies the automated tag-triggered release pipeline
(GitHub Actions → pub.dev).

## 0.1.0 - 2026-07-23

Initial release.

### Added
- Public API (`package:blifi/blifi.dart`): `BlifiProvisioning`
  (`scanForDevices`, `connect`) and `BlifiProvisioningSession`
  (`scanWifiNetworks`, `sendCredentials`, `statusStream`, `disconnect`), with
  models (`BlifiDevice`, `WifiNetwork`/`WifiAuthMode`, `ProvisioningStatus`/
  `ProvisioningState`, `BlifiDeviceInfo`) and a typed exception hierarchy
  (`BlifiException` and subtypes). Fully dartdoc'd.
- Package core (`lib/src`), the Dart mirror of the firmware:
  - `protocol`: framing (8-byte big-endian header, chunking, reassembly), the
    JSON payload codecs, and shared constants/status codes.
  - `crypto`: `BlifiSession` - X25519 + HKDF-SHA256 + AES-256-GCM + HMAC-SHA256
    (app role) via `package:cryptography`, matching `docs/security.md`.
  - `ble`: `flutter_blue_plus` transport (scan/connect/MTU/chunked notify +
    reassembly) and `ProvisioningSession` (handshake, Wi-Fi scan, credential
    send, status stream).
- Host tests: RFC 7748 / RFC 5869 known-answer vectors plus blifi session
  vectors verified against an independent OpenSSL oracle (byte-identical to the
  firmware), framing and JSON round-trips.
- Minimal example app hosting a live interop integration test against the
  firmware (handshake + encrypted Wi-Fi scan), verified end-to-end on hardware.
