# demo_app (Flutter demo / boilerplate)

A polished, intentionally generic Flutter app that demonstrates the full
provisioning flow end-to-end (BLE scan → connect → Wi-Fi list → password entry
→ live progress → success/failure → reconnection) and doubles as a reusable
starting point. Depends on the `blifi` package via a local path dependency.

Android build target: `compileSdk`/`targetSdk` 36, `minSdkVersion` 30. Primary
testing is on a physical Android device.

> **Not yet implemented.** Scaffolding only — built in a later phase.
