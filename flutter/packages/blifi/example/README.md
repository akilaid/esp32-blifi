# blifi example

Minimal end-to-end demo of the [`blifi`](https://pub.dev/packages/blifi) package:
scan for nearby blifi devices, connect with the device's Proof-of-Possession,
list the Wi-Fi networks it sees, send credentials, and watch the provisioning
status until the device is online.

## Prerequisites

- A physical Android/iOS device with BLE (emulators have no Bluetooth).
- An ESP32 nearby running the
  [blifi firmware component](https://github.com/akilaid/esp32-blifi/tree/main/firmware/components/blifi)
  in unprovisioned mode, and its Proof-of-Possession code (printed on the
  device's serial console or QR sticker).

## Run

```bash
flutter run
```

Everything lives in [`lib/main.dart`](https://github.com/akilaid/esp32-blifi/blob/main/flutter/packages/blifi/example/lib/main.dart).
For a polished, full-featured app (QR provisioning, Material 3 UI), see the
[demo app](https://github.com/akilaid/esp32-blifi/tree/main/flutter/apps/demo_app)
in the same repository.
