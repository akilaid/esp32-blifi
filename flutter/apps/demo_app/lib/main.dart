import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

import 'provisioning_controller.dart';
import 'screens/progress_screen.dart';
import 'screens/result_screen.dart';
import 'screens/scan_screen.dart';
import 'screens/wifi_screen.dart';

void main() => runApp(const BlifiDemoApp());

/// A generic, reusable demo of BLE Wi-Fi provisioning with the `blifi` package.
class BlifiDemoApp extends StatefulWidget {
  const BlifiDemoApp({super.key});

  @override
  State<BlifiDemoApp> createState() => _BlifiDemoAppState();
}

class _BlifiDemoAppState extends State<BlifiDemoApp> {
  final _controller = ProvisioningController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'blifi',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(colorSchemeSeed: Colors.blue, useMaterial3: true),
      home: _Home(_controller),
    );
  }
}

class _Home extends StatelessWidget {
  const _Home(this.controller);
  final ProvisioningController controller;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: controller,
      builder: (context, _) => switch (controller.stage) {
        ProvisioningStage.permissions => _PermissionGate(controller),
        ProvisioningStage.scanning => ScanScreen(controller),
        ProvisioningStage.connecting => const _Loading('Connecting…'),
        ProvisioningStage.wifiList => WifiScreen(controller),
        ProvisioningStage.provisioning => ProgressScreen(controller),
        ProvisioningStage.success => ResultScreen(controller, success: true),
        ProvisioningStage.failure => ResultScreen(controller, success: false),
      },
    );
  }
}

class _PermissionGate extends StatelessWidget {
  const _PermissionGate(this.controller);
  final ProvisioningController controller;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              const Icon(Icons.bluetooth, size: 96),
              const SizedBox(height: 16),
              Text('blifi provisioning',
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.headlineSmall),
              const SizedBox(height: 8),
              const Text(
                'Set up an ESP32’s Wi-Fi over Bluetooth. This app needs '
                'Bluetooth (and, on older Android, Location) permission to scan.',
                textAlign: TextAlign.center,
              ),
              if (controller.errorMessage != null) ...[
                const SizedBox(height: 16),
                Text(controller.errorMessage!,
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Theme.of(context).colorScheme.error)),
                TextButton(onPressed: openAppSettings, child: const Text('Open settings')),
              ],
              const SizedBox(height: 32),
              FilledButton.icon(
                key: const Key('grant_button'),
                onPressed: controller.requestPermissions,
                icon: const Icon(Icons.check),
                label: const Text('Grant & scan'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _Loading extends StatelessWidget {
  const _Loading(this.label);
  final String label;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const CircularProgressIndicator(),
            const SizedBox(height: 24),
            Text(label),
          ],
        ),
      ),
    );
  }
}
