// Live on-device test of the QR auto-find + connect logic against the real
// firmware (the camera capture itself can't be automated). Drives the
// controller directly with a payload as if a QR had been scanned.
//
//   flutter test integration_test/app_flow_test.dart -d <android-id> \
//     --dart-define=POP=XXXXXXXX
//
// Requires the ESP32 advertising (unprovisioned), phone Bluetooth + Location on,
// and BLE/location permissions granted (adb install -g).
import 'package:blifi_demo/provisioning_controller.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  const pop = String.fromEnvironment('POP');

  testWidgets('QR path: auto-find blifi-2F04 + connect reaches the Wi-Fi list',
      (tester) async {
    await tester.pumpWidget(const SizedBox());
    final controller = ProvisioningController();
    addTearDown(controller.dispose);

    await tester.runAsync(
      () => controller.connectViaQr('blifi-2F04', pop).timeout(const Duration(seconds: 90)),
    );

    expect(controller.stage, ProvisioningStage.wifiList,
        reason: 'error: ${controller.errorMessage}');
    expect(controller.networks, isNotEmpty);
    // ignore: avoid_print
    print('QR-INTEROP: reached Wi-Fi list with ${controller.networks.length} networks');
  }, timeout: const Timeout(Duration(seconds: 120)));
}
