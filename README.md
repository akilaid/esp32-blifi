# esp32-blifi

BLE-based Wi-Fi provisioning for ESP32: replaces the hotspot/captive-portal flow
with a companion app that connects over Bluetooth Low Energy, negotiates an
encrypted session (X25519 + AES-256-GCM, gated by a Proof-of-Possession), sends
Wi-Fi credentials, and gets the device online.

One codebase, four independently-versioned artifacts. The **ESP-IDF component is the
single source of truth**; the Arduino library wraps it and the Flutter package
mirrors its wire protocol byte-for-byte.

## What's here

| Path | Artifact | Build |
|------|----------|-------|
| [`firmware/components/blifi/`](firmware/components/blifi) | Core ESP-IDF component (NimBLE, PSA crypto, Wi-Fi, provisioning state machine, reset-pin hard reset) | `idf.py build` (example) |
| [`arduino/Blifi/`](arduino/Blifi) | Arduino-style wrapper over the component | PlatformIO (`pio run`) - see its README |
| [`flutter/packages/blifi/`](flutter/packages/blifi) | Publishable Flutter/Dart package (`blifi`) | `flutter test` |
| [`flutter/apps/demo_app/`](flutter/apps/demo_app) | Polished demo / boilerplate app (Material 3 Expressive, QR provisioning) | `flutter run` |

## Quick start

```bash
# Firmware (via the example project)
cd firmware/components/blifi/examples/esp-idf-example && idf.py build flash monitor

# Demo app (physical Android device with BLE)
cd flutter/apps/demo_app && flutter run
```

Provision by scanning the device's QR (or entering its Proof-of-Possession) in the
app, then picking a Wi-Fi network. Details in each artifact's `README.md`.

## Documentation

- [`docs/protocol-spec.md`](docs/protocol-spec.md) - GATT UUIDs, framing, message schema
- [`docs/security.md`](docs/security.md) - threat model, crypto design, PoP handling
- [`docs/adr/`](docs/adr) - architecture decision records

## Distribution

- **Flutter package** → pub.dev (id `blifi`)
- **ESP-IDF component** → ESP Component Registry (`akilaid/blifi`)
- **Arduino library** → published via a generated mirror repo,
  [`esp32-blifi-arduino`](https://github.com/akilaid/esp32-blifi-arduino), for the
  Arduino Library Manager (see [ADR 0006](docs/adr/0006-arduino-distribution-mirror.md))

Publishing is automated on merge to `main` (a version bump is the release trigger);
see [RELEASING.md](RELEASING.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and, for the release flow,
[RELEASING.md](RELEASING.md). Each artifact has its own `CHANGELOG.md` and semver.

## License

[MIT](LICENSE).
