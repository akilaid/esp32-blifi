# Changelog — demo_app (Flutter demo / boilerplate)

All notable changes to the demo app are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this app is versioned
independently under [Semantic Versioning](https://semver.org/).

## 0.1.0 - 2026-07-23

Initial release.

### Added
- Full provisioning UX on the `blifi` package: permission gate → device scan →
  connect (PoP) → Wi-Fi list (signal strength + lock) → password entry → live
  progress → success (SSID + IP) / failure with retry → start over.
- Plain `ChangeNotifier` controller (no state-management dependency); Android
  `minSdk 30` / `compile+targetSdk 36`; runtime BLE permissions via
  `permission_handler`.
- Basic widget tests and an on-device integration test of the flow.
