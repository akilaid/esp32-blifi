# ADR 0003 - Single firmware codebase, thin Arduino wrapper (and event model)

- **Status:** Accepted (2026-07-23)
- **Related:** [`../../firmware/components/blifi/`](../../firmware/components/blifi/), [`../../arduino/Blifi/`](../../arduino/Blifi/)

## Context

The provisioning logic must be usable from both ESP-IDF projects and the Arduino
(`arduino-esp32`) ecosystem. Maintaining two independent implementations of the
same BLE + crypto + state-machine logic would guarantee divergence and double
the surface for interop bugs against the Flutter package.

## Decision

Keep **one ESP-IDF component (`blifi`) as the single source of truth**, and ship
the Arduino library (`Blifi`) as a **thin C++ wrapper** over that component's C
API - no provisioning logic duplicated. `arduino-esp32` 3.x (IDF5-based) can pull
the component in via `idf_component.yml`, so the wrapper only adapts the API
shape.

**Event model (folded in here):** the component exposes an **`esp_event`-based
custom event base** (`BLIFI_EVENT` with events like `BLE_CONNECTED`,
`CREDENTIALS_RECEIVED`, `WIFI_CONNECTED`, …) rather than ad-hoc callbacks. This
keeps the surface idiomatic for ESP-IDF users **and** gives the Arduino wrapper a
single, stable thing to translate into `std::function` callbacks
(`onStatusChanged`, `onProvisioned`, …). It has no separate ADR file because
its rationale is inseparable from the single-codebase approach.

## Consequences

- One place to fix bugs and evolve the protocol; the Arduino library tracks it
  automatically.
- Hard requirement: **arduino-esp32 3.x** (IDF5-based). Older cores cannot pull
  ESP-IDF components this way - called out in the Arduino README so users get a
  clear message instead of a confusing build failure.
- The component's public C API (`esp_event` base + a handful of functions) is the
  contract the wrapper depends on, so it must stay stable.

## Alternatives considered

- **Two native implementations (one pure-Arduino, one ESP-IDF):** rejected -
  duplication and inevitable drift.
- **Callback-only C API (no `esp_event`):** less idiomatic for ESP-IDF consumers
  and gives the wrapper a larger, less stable surface to adapt.
