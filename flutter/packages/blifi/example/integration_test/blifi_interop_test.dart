// Live interop test against the real blifi firmware on an ESP32.
//
// Run with the device's current PoP:
//   flutter test integration_test/blifi_interop_test.dart -d <android-id> \
//     --dart-define=POP=XXXXXXXX
//
// Requires: the ESP32 powered and advertising (unprovisioned), the phone's
// Bluetooth + Location on, and ACCESS_FINE_LOCATION granted.
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

// ignore: implementation_imports
import 'package:blifi/src/ble/ble_transport.dart';
// ignore: implementation_imports
import 'package:blifi/src/ble/provisioning_session.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  const pop = String.fromEnvironment('POP');

  testWidgets('live handshake + Wi-Fi scan against firmware', (tester) async {
    expect(await FlutterBluePlus.isSupported, isTrue);

    if (await FlutterBluePlus.adapterState.first != BluetoothAdapterState.on) {
      await FlutterBluePlus.turnOn();
      await FlutterBluePlus.adapterState
          .firstWhere((s) => s == BluetoothAdapterState.on)
          .timeout(const Duration(seconds: 15));
    }

    final device = await BlifiTransport.findDevice(timeout: const Duration(seconds: 15));
    expect(device, isNotNull,
        reason: 'no blifi device found — is the ESP32 advertising and Location on?');

    final session = await ProvisioningSession.connect(device!, pop: pop);
    // Reaching here means the device confirmation tag verified (mutual auth).
    expect(session.deviceInfo.name.toLowerCase().startsWith('blifi'), isTrue);
    expect(session.deviceInfo.proto, 1);

    final networks = await session.scanWifiNetworks();
    // ignore: avoid_print
    print('INTEROP: handshake OK, ${networks.length} Wi-Fi networks: '
        '${networks.map((n) => n.ssid).take(5).toList()}');
    expect(networks, isNotEmpty, reason: 'expected a decrypted Wi-Fi scan list');

    await session.disconnect();
  }, timeout: const Timeout(Duration(seconds: 120)));
}
