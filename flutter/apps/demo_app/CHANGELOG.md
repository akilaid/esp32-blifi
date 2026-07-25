# Changelog - demo_app (Flutter demo / boilerplate)

All notable changes to the demo app are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this app is versioned
independently under [Semantic Versioning](https://semver.org/).

## Unreleased

### Fixed
- QR scanner showed a blank "!" icon instead of a camera preview on `compileSdk 36`
  devices. Upgraded `mobile_scanner` 5.2.3 → 7.4.0 (newer CameraX), migrated the
  scan screen to the v7 explicit-start lifecycle, and added an `errorBuilder` that
  surfaces the real error code with retry / manual-entry fallbacks.

## 0.1.0 - 2026-07-23

Initial release.

### Added
- Full provisioning UX on the `blifi` package: home → device scan / **QR scan** →
  connect (PoP) → Wi-Fi list (signal strength + lock) → password entry → live
  progress → success (SSID + IP) / failure with retry → start over.
- **QR-code provisioning**: scan `blifi://provision?name=…&pop=…` to auto-find
  the device and connect hands-free (`mobile_scanner`; reference payload parser
  in `lib/qr_payload.dart`). Manual device selection remains available.
- **Material 3 Expressive** UI: dynamic color (Material You) with a seed
  fallback, light/dark, smooth animated transitions, and descriptive step-based
  progress so nothing feels stuck.
- Plain `ChangeNotifier` controller (no state-management dependency); Android
  `minSdk 30` / `compile+targetSdk 36`; runtime BLE + camera permissions via
  `permission_handler`.
- Widget tests, `QrPayload` unit tests, and an on-device integration test of the
  QR auto-find + connect path.
