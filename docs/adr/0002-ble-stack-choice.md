# ADR 0002 - BLE stack (firmware)

- **Status:** Accepted (2026-07-23)
- **Related:** [`../protocol-spec.md`](../protocol-spec.md)

## Context

The firmware needs a GATT server for provisioning. This project uses **BLE only**
- there is no requirement for Bluetooth Classic (BR/EDR). ESP-IDF offers two host
stacks: **NimBLE** (`CONFIG_BT_NIMBLE_ENABLED`) and **Bluedroid**.

## Decision

Use **NimBLE**.

## Consequences

- Noticeably lower RAM and flash footprint than Bluedroid, leaving more headroom
  on constrained ESP32 modules.
- API is BLE-focused and maps cleanly onto the small set of GATT characteristics
  the protocol needs.
- No Bluetooth Classic - acceptable, since provisioning is BLE-only.
- The BLE transport layer (`ble_transport/`) is written against NimBLE APIs; a
  future port to another stack would be isolated to that module.

## Alternatives considered

- **Bluedroid:** required only if Bluetooth Classic (A2DP/SPP/etc.) were needed;
  larger footprint. Rejected for a BLE-only use case.
