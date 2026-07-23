import 'package:flutter/material.dart';

import '../provisioning_controller.dart';

/// Success or failure result, with follow-up actions.
class ResultScreen extends StatelessWidget {
  const ResultScreen(this.controller, {super.key, required this.success});

  final ProvisioningController controller;
  final bool success;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Icon(
                success ? Icons.check_circle : Icons.error,
                key: Key(success ? 'result_success' : 'result_failure'),
                size: 96,
                color: success ? Colors.green : scheme.error,
              ),
              const SizedBox(height: 24),
              Text(
                success ? 'Connected!' : 'Provisioning failed',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.headlineSmall,
              ),
              const SizedBox(height: 8),
              Text(
                success
                    ? '${controller.connectedSsid} · ${controller.ipAddress ?? ''}'
                    : (controller.errorMessage ?? 'Unknown error'),
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodyLarge,
              ),
              const SizedBox(height: 40),
              if (!success)
                FilledButton(
                  key: const Key('retry_button'),
                  onPressed: controller.retry,
                  child: const Text('Try another network'),
                ),
              const SizedBox(height: 12),
              OutlinedButton(
                key: const Key('startover_button'),
                onPressed: controller.startOver,
                child: Text(success ? 'Provision another device' : 'Start over'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
