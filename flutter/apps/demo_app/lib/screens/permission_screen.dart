import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import 'package:permission_handler/permission_handler.dart';

import '../provisioning_controller.dart';

/// Intro + BLE permission request.
class PermissionScreen extends StatelessWidget {
  const PermissionScreen(this.controller, {super.key});

  final ProvisioningController controller;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(28),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Center(
                child: Container(
                  width: 120,
                  height: 120,
                  decoration: BoxDecoration(
                    color: scheme.primaryContainer,
                    shape: BoxShape.circle,
                  ),
                  child: Icon(Icons.bluetooth_rounded, size: 60, color: scheme.onPrimaryContainer),
                )
                    .animate(onPlay: (c) => c.repeat(reverse: true))
                    .scaleXY(begin: 1, end: 1.06, duration: 1600.ms, curve: Curves.easeInOut),
              ),
              const SizedBox(height: 32),
              Text('Set up your device',
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 12),
              Text(
                'Connect an ESP32 to Wi-Fi over Bluetooth. We need Bluetooth '
                '(and, on older Android, Location) to find nearby devices.',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodyLarge?.copyWith(color: scheme.onSurfaceVariant),
              ),
              if (controller.errorMessage != null) ...[
                const SizedBox(height: 20),
                Text(controller.errorMessage!,
                    textAlign: TextAlign.center, style: TextStyle(color: scheme.error)),
                TextButton(onPressed: openAppSettings, child: const Text('Open settings')),
              ],
              const SizedBox(height: 40),
              FilledButton.icon(
                key: const Key('grant_button'),
                onPressed: controller.requestPermissions,
                icon: const Icon(Icons.arrow_forward_rounded),
                label: const Text('Grant & continue'),
              ),
            ]
                .animate(interval: 90.ms)
                .fadeIn(duration: 500.ms, curve: Curves.easeOut)
                .slideY(begin: 0.12, end: 0, curve: Curves.easeOutCubic),
          ),
        ),
      ),
    );
  }
}
