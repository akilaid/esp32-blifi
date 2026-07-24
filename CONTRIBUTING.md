# Contributing to esp32-blifi

Thanks for your interest! This is a single repository holding four
independently-versioned artifacts that evolve together:

| Artifact | Path | Toolchain |
|----------|------|-----------|
| ESP-IDF component (`blifi`) | `firmware/components/blifi/` | ESP-IDF ≥ 5.3 |
| Arduino library (`Blifi`) | `arduino/Blifi/` | PlatformIO (arduino-esp32 3.x) |
| Flutter package (`blifi`) | `flutter/packages/blifi/` | Flutter ≥ 3.16 |
| Demo app | `flutter/apps/demo_app/` | Flutter |

The **ESP-IDF component is the single source of truth** — the Arduino library is a
thin wrapper over it, and the Flutter package mirrors its protocol byte-for-byte.
`arduino/Blifi/` is mirrored to
[`esp32-blifi-arduino`](https://github.com/akilaid/esp32-blifi-arduino)
automatically for the Arduino Library Manager (see
[ADR 0006](docs/adr/0006-arduino-distribution-mirror.md)) — never edit that repo
directly.

## Before you start

Read [`docs/plan.md`](docs/plan.md) for the architecture and the decisions already
settled (in [`docs/adr/`](docs/adr/)). New architectural decisions go in a new ADR
following the existing numbering.

## Building & testing

```bash
# Firmware component (via the example project)
cd firmware/components/blifi/examples/esp-idf-example && idf.py build

# Arduino library (PlatformIO)
cd arduino/Blifi && pio run

# Flutter package
cd flutter/packages/blifi && flutter pub get && flutter analyze && flutter test

# Demo app
cd flutter/apps/demo_app && flutter pub get && flutter analyze && flutter test
```

CI (GitHub Actions) runs the equivalent checks on every PR.

## Pull requests

- Keep changes scoped to one artifact where possible. A protocol change that spans
  firmware + the Flutter package should land in **one PR** so the reference docs and
  both implementations stay in lockstep.
- Update the relevant `CHANGELOG.md` (each artifact has its own) under
  `## [Unreleased]`, and bump the artifact's version when cutting a release.
- Match the surrounding code style; run the analyzers/formatters before pushing.
- Make sure CI is green.

## Releasing

- **Flutter package / ESP component:** an explicit publish step (not automated on
  merge).
- **Arduino library:** bump `version` in `arduino/Blifi/library.properties`, then a
  maintainer runs the manual **`arduino-release`** workflow, which tags and releases
  the mirror repo. See [ADR 0006](docs/adr/0006-arduino-distribution-mirror.md).

## Reporting issues

Use the issue templates. For security-sensitive reports, please avoid filing a
public issue with exploit details — see [`docs/security.md`](docs/security.md).

By contributing, you agree your contributions are licensed under the
repository's [MIT License](LICENSE).
