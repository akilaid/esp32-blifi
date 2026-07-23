# blifi demo app

A polished, intentionally **generic** Flutter app demonstrating the full BLE
Wi-Fi provisioning flow with the [`blifi`](../../packages/blifi) package — and a
reusable boilerplate to start your own provisioning UI from.

**Flow:** grant permissions → scan for devices → connect (enter the device's
Proof-of-Possession) → pick a Wi-Fi network (signal strength + lock) → enter the
password → live progress → success (SSID + IP) or failure with retry → start
over / provision another.

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
- State is a plain `ChangeNotifier` (`lib/provisioning_controller.dart`) — no
  state-management package, so it's easy to adapt.
- Android: `minSdk 30`, `compile/targetSdk 36`. BLE permissions are requested at
  runtime via `permission_handler`.
- "Start over" disconnects and re-scans (app-side). Erasing the device's stored
  credentials is a device-side action (serial `reset`), not part of the BLE
  protocol.
