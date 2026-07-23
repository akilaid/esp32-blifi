// Live interop test against the real blifi firmware, driving the PUBLIC API.
//
// Run with the device's current PoP:
//   flutter test integration_test/blifi_interop_test.dart -d <android-id> \
//     --dart-define=POP=XXXXXXXX
//
// Requires: the ESP32 powered and advertising (unprovisioned), the phone's
// Bluetooth + Location on, and ACCESS_FINE_LOCATION granted.
import 'package:blifi/blifi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  const pop = String.fromEnvironment('POP');

  testWidgets('public API: handshake + Wi-Fi scan against firmware', (tester) async {
    const blifi = BlifiProvisioning();

    final device = await blifi
        .scanForDevices(timeout: const Duration(seconds: 15))
        .first
        .timeout(const Duration(seconds: 20));
    expect(device.name.toLowerCase().startsWith('blifi'), isTrue);

    final session = await blifi.connect(device, proofOfPossession: pop);
    // Reaching here means the mutual confirmation verified.
    expect(session.deviceInfo.protocolVersion, 1);

    final networks = await session.scanWifiNetworks();
    // ignore: avoid_print
    print('INTEROP: handshake OK, ${networks.length} networks: '
        '${networks.map((n) => n.ssid).take(5).toList()}');
    expect(networks, isNotEmpty);

    await session.disconnect();
  }, timeout: const Timeout(Duration(seconds: 120)));
}
