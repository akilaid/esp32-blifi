# WiFiBleProvisioning (Arduino library)

A thin C++ wrapper over the `wifi_ble_prov` ESP-IDF component — no logic is
duplicated. Translates the component's `esp_event`-based API into idiomatic
Arduino `std::function` callbacks (`begin`, `onStatusChanged`, `onProvisioned`,
`resetCredentials`, `isProvisioned`).

> **Requires arduino-esp32 3.x** (IDF5-based), which can pull in ESP-IDF
> components via `idf_component.yml`. Older cores are not supported.

> **Not yet implemented.** Scaffolding only — implemented in a later phase,
> after the component API is stable.
