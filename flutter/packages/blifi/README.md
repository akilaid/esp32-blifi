# blifi

BLE-based Wi-Fi provisioning for ESP32 — hand Wi-Fi credentials to a device over
an **encrypted Bluetooth Low Energy** session instead of a hotspot/captive
portal. This Dart package is the phone side; it interoperates byte-for-byte with
the [`blifi` ESP-IDF firmware component](https://github.com/akilaid/esp32-blifi).

- 🔒 **Encrypted**: X25519 key agreement + AES-256-GCM, with a Proof-of-Possession
  so credentials are never exposed and an active MITM can't succeed.
- 📶 **Full flow**: scan for devices, connect, list the device's Wi-Fi networks,
  send credentials, and stream live connection status.
- 🧩 **Small, typed API** with typed exceptions — no string-matching errors.

## Install

```yaml
dependencies:
  blifi: ^0.1.0
```

## Permissions

This package uses [`flutter_blue_plus`](https://pub.dev/packages/flutter_blue_plus)
for BLE. Add the platform permissions and request them at runtime (e.g. with
[`permission_handler`](https://pub.dev/packages/permission_handler)).

**Android** — `android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<!-- Android 11 and below -->
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
```

**iOS** — `ios/Runner/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Connect to your device to set up Wi-Fi.</string>
```

`BlifiProvisioning` throws `BleUnavailableException` if Bluetooth is off or
unsupported, so you never fail silently.

## Usage

```dart
import 'package:blifi/blifi.dart';

final blifi = BlifiProvisioning();

// 1. Discover a device.
final device = await blifi.scanForDevices().first;

// 2. Connect and complete the secure handshake with the device's PoP.
final session = await blifi.connect(device, proofOfPossession: 'K7M2QP9X');

// 3. List the device's visible Wi-Fi networks.
final networks = await session.scanWifiNetworks();

// 4. Send credentials and watch progress.
session.statusStream.listen((s) {
  print('${s.state}${s.ipAddress != null ? ' @ ${s.ipAddress}' : ''}');
});
await session.sendCredentials('HomeWiFi', 'password');

// 5. Done.
await session.disconnect();
```

Errors are typed: catch `AuthenticationException` (wrong PoP),
`WifiConnectionException` (with a `ProvisioningState`), `BleConnectionException`,
or `ProvisioningTimeoutException` — all subtypes of `BlifiException`.

See [`example/`](example/) for a runnable app.

## License

[MIT](LICENSE).
