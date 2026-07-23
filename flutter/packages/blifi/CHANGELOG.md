# Changelog — blifi (Flutter package)

All notable changes to the Flutter package are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this package is
versioned independently under [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Package core (`lib/src`), the Dart mirror of the firmware:
  - `protocol`: framing (8-byte big-endian header, chunking, reassembly), the
    JSON payload codecs, and shared constants/status codes.
  - `crypto`: `BlifiSession` — X25519 + HKDF-SHA256 + AES-256-GCM + HMAC-SHA256
    (app role) via `package:cryptography`, matching `docs/security.md`.
  - `ble`: `flutter_blue_plus` transport (scan/connect/MTU/chunked notify +
    reassembly) and `ProvisioningSession` (handshake, Wi-Fi scan, credential
    send, status stream).
- Host tests: RFC 7748 / RFC 5869 known-answer vectors plus blifi session
  vectors verified against an independent OpenSSL oracle (byte-identical to the
  firmware), framing and JSON round-trips.
- Minimal example app hosting a live interop integration test against the
  firmware (handshake + encrypted Wi-Fi scan), verified end-to-end on hardware.
