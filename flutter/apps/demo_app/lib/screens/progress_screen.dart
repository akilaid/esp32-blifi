import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';

import '../provisioning_controller.dart';
import '../widgets/step_timeline.dart';

/// Live provisioning progress as a step timeline driven by the status stream.
class ProgressScreen extends StatelessWidget {
  const ProgressScreen(this.controller, {super.key});

  final ProvisioningController controller;

  int get _activeIndex {
    final state = controller.status?.state;
    return (state == ProvisioningState.credentialsReceived ||
            state == ProvisioningState.wifiConnecting)
        ? 1
        : 0;
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Scaffold(
      appBar: AppBar(title: const Text('Setting up')),
      body: Padding(
        padding: const EdgeInsets.all(28),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const SizedBox(height: 8),
            Text('Connecting ${controller.connectedSsid ?? 'your device'}',
                style: theme.textTheme.headlineSmall),
            const SizedBox(height: 4),
            Text('This usually takes a few seconds.',
                style: theme.textTheme.bodyMedium
                    ?.copyWith(color: theme.colorScheme.onSurfaceVariant)),
            const SizedBox(height: 40),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: StepTimeline(
                  activeIndex: _activeIndex,
                  steps: const [
                    'Sending credentials',
                    'Joining the network',
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
