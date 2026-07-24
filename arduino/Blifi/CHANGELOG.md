# Changelog — Blifi (Arduino library)

All notable changes to the Arduino library are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this library is
versioned independently under [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Initial `Blifi` wrapper: a thin C++ Arduino API over the `blifi` ESP-IDF
  component (`begin`, `onProvisioned`, `onStatusChanged`, `isProvisioned`,
  `resetCredentials`, `pop`, `statusString`, plus `onDataResetRequested` /
  `wasHardReset` hooks). Public header `Blifi.h`; global `Blifi` instance.
- Ships as a **PlatformIO** project (`framework = arduino, espidf` on the
  pioarduino platform) with `platformio.ini`, `sdkconfig.defaults` (NimBLE on),
  `partitions.csv`, and a `BasicProvisioning` sketch (`src/main.cpp`). The `blifi`
  component is pulled via a relative `src/idf_component.yml` path dependency.
- README documenting why PlatformIO is required (the stock Arduino IDE core is
  Bluedroid-only and can't build the NimBLE component or change bootloader
  config), build/upload steps, and the API.

### Notes
- Verified on hardware: provisioning → Wi-Fi connect → `Online!` with both
  callbacks firing. The IDF-6.0 `blifi` component builds unchanged on ESP-IDF 5.5.
- The reset-pin hard reset is a bootloader feature and is not enabled in this
  basic setup (`wasHardReset()` returns false); software `resetCredentials()`
  works. See the component README to enable the pin reset.
