import 'package:blifi_demo/main.dart';
import 'package:blifi_demo/provisioning_controller.dart';
import 'package:blifi_demo/screens/result_screen.dart';
import 'package:blifi_demo/widgets/signal_icon.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('app opens on the permission gate', (tester) async {
    await tester.pumpWidget(const BlifiDemoApp());
    expect(find.byKey(const Key('grant_button')), findsOneWidget);
    expect(find.text('blifi provisioning'), findsOneWidget);
  });

  testWidgets('success result shows the network and actions', (tester) async {
    final controller = ProvisioningController()..connectedSsid = 'HomeWiFi';
    await tester.pumpWidget(
      MaterialApp(home: ResultScreen(controller, success: true)),
    );
    expect(find.byKey(const Key('result_success')), findsOneWidget);
    expect(find.text('Connected!'), findsOneWidget);
    expect(find.textContaining('HomeWiFi'), findsOneWidget);
    expect(find.byKey(const Key('startover_button')), findsOneWidget);
  });

  testWidgets('failure result shows the error and retry', (tester) async {
    final controller = ProvisioningController()..errorMessage = 'Wrong Wi-Fi password.';
    await tester.pumpWidget(
      MaterialApp(home: ResultScreen(controller, success: false)),
    );
    expect(find.byKey(const Key('result_failure')), findsOneWidget);
    expect(find.text('Wrong Wi-Fi password.'), findsOneWidget);
    expect(find.byKey(const Key('retry_button')), findsOneWidget);
  });

  testWidgets('signal icon reflects strength', (tester) async {
    Future<void> pump(int rssi) => tester.pumpWidget(
          MaterialApp(home: Scaffold(body: SignalIcon(rssi))),
        );
    await pump(-50);
    expect(find.byIcon(Icons.signal_wifi_4_bar), findsOneWidget);
    await pump(-90);
    expect(find.byIcon(Icons.signal_wifi_0_bar), findsOneWidget);
  });
}
