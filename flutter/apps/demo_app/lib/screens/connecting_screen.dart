import 'package:flutter/material.dart';

import '../provisioning_controller.dart';

/// A descriptive "connecting" state: an expressive indicator plus a smoothly
/// changing label so the user always sees forward motion.
class ConnectingScreen extends StatelessWidget {
  const ConnectingScreen(this.controller, {super.key});

  final ProvisioningController controller;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Scaffold(
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const SizedBox(
              width: 72,
              height: 72,
              child: CircularProgressIndicator(strokeWidth: 6),
            ),
            const SizedBox(height: 36),
            AnimatedSwitcher(
              duration: const Duration(milliseconds: 350),
              child: Padding(
                key: ValueKey(controller.connectingLabel),
                padding: const EdgeInsets.symmetric(horizontal: 32),
                child: Text(
                  controller.connectingLabel,
                  textAlign: TextAlign.center,
                  style: theme.textTheme.titleMedium,
                ),
              ),
            ),
            const SizedBox(height: 10),
            Text('Keeping your device close helps.',
                style: theme.textTheme.bodySmall
                    ?.copyWith(color: theme.colorScheme.onSurfaceVariant)),
          ],
        ),
      ),
    );
  }
}
