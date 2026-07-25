# blifi demo app

A polished, intentionally **generic** Flutter app demonstrating the full BLE
Wi-Fi provisioning flow with the [`blifi`](../../packages/blifi) package - and a
reusable boilerplate to start your own provisioning UI from.

**Flow:** grant permissions → **Home** → either **Scan a QR code**
(`blifi://provision?name=…&pop=…`) to auto-find the device and connect
hands-free, **or Find devices** manually and enter the Proof-of-Possession →
pick a Wi-Fi network (signal strength + lock) → enter the password → live
progress → success (SSID + IP) or failure with retry → start over.

Built with **Material 3 Expressive** - dynamic color (Material You), light/dark,
and smooth motion with descriptive progress at every step.

## Run

```bash
flutter pub get
flutter run -d <your-android-device>   # a physical device with BLE
```

The device's Proof-of-Possession is printed on the ESP32's serial output on
first boot.

## Notes
- Depends on the package via a **path dependency**; switch to a pub.dev version
  once published.
- State is a plain `ChangeNotifier` (`lib/provisioning_controller.dart`) - no
  state-management package, so it's easy to adapt.
- Android: `minSdk 30`, `compile/targetSdk 36`. BLE + camera permissions are
  requested at runtime via `permission_handler`.
- QR scanning uses `mobile_scanner`; the QR payload format
  (`lib/qr_payload.dart`) is a **reference** - adapt it to your factory's format.
  QR + camera live in the app only; the `blifi` package stays QR-agnostic.
- "Start over" disconnects and re-scans (app-side). Erasing the device's stored
  credentials is a device-side action (serial `reset`), not part of the BLE
  protocol.
