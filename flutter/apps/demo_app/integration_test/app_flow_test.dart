// Live on-device UI flow test against the real blifi firmware, up to the
// failure path (a deliberately wrong Wi-Fi password — no real secrets).
//
//   flutter test integration_test/app_flow_test.dart -d <android-id> \
//     --dart-define=POP=XXXXXXXX
//
// Requires the ESP32 advertising (unprovisioned), phone Bluetooth + Location on,
// and BLE/location permissions granted (adb install -g).
import 'package:blifi_demo/main.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  const pop = String.fromEnvironment('POP');

  // Pump repeatedly (never pumpAndSettle — the app shows infinite progress
  // spinners) until [finder] matches or we time out.
  Future<void> waitFor(WidgetTester t, Finder finder,
      {Duration timeout = const Duration(seconds: 25)}) async {
    final end = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(end)) {
      await t.pump(const Duration(milliseconds: 400));
      if (finder.evaluate().isNotEmpty) return;
    }
    throw TestFailure('timed out waiting for: $finder');
  }

  testWidgets('full flow to failure (wrong Wi-Fi password)', (tester) async {
    await tester.pumpWidget(const BlifiDemoApp());
    await tester.pump();

    // Grant (already granted on CI device) → scan.
    await tester.tap(find.byKey(const Key('grant_button')));
    await tester.pump();

    // Discover the device and open the PoP dialog.
    final deviceTile = find.textContaining('blifi-');
    await waitFor(tester, deviceTile);
    await tester.tap(deviceTile.first);
    await tester.pump();

    await tester.enterText(find.byKey(const Key('pop_field')), pop);
    await tester.tap(find.byKey(const Key('pop_ok')));
    await tester.pump();

    // Wait for the Wi-Fi list, then pick a secured network.
    final lock = find.byIcon(Icons.lock_outline);
    await waitFor(tester, lock, timeout: const Duration(seconds: 30));
    final securedTile =
        find.ancestor(of: lock.first, matching: find.byType(ListTile)).first;
    await tester.tap(securedTile);
    await tester.pump();

    // Enter a wrong password.
    await tester.enterText(find.byKey(const Key('password_field')), 'definitely-wrong-pw');
    await tester.tap(find.byKey(const Key('password_ok')));
    await tester.pump();

    // Expect the failure screen (firmware fast-fails on the wrong password).
    await waitFor(tester, find.byKey(const Key('result_failure')),
        timeout: const Duration(seconds: 40));
    expect(find.byKey(const Key('result_failure')), findsOneWidget);
    // ignore: avoid_print
    print('DEMO FLOW: reached failure screen as expected');
  }, timeout: const Timeout(Duration(seconds: 150)));
}
