import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

import '../provisioning_controller.dart';

/// Success or failure result, with follow-up actions.
class ResultScreen extends StatelessWidget {
  const ResultScreen(this.controller, {super.key, required this.success});

  final ProvisioningController controller;
  final bool success;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final color = success ? Colors.green : scheme.error;
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
                  width: 128,
                  height: 128,
                  decoration: BoxDecoration(
                    color: color.withValues(alpha: 0.12),
                    shape: BoxShape.circle,
                  ),
                  child: Icon(
                    success ? Icons.check_rounded : Icons.error_outline_rounded,
                    key: Key(success ? 'result_success' : 'result_failure'),
                    size: 72,
                    color: color,
                  ),
                )
                    .animate()
                    .scale(
                      begin: const Offset(0.4, 0.4),
                      end: const Offset(1, 1),
                      duration: 480.ms,
                      curve: Curves.easeOutBack,
                    )
                    .fadeIn(duration: 240.ms),
              ),
              const SizedBox(height: 32),
              Text(
                success ? 'Connected!' : 'Couldn’t connect',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.headlineMedium,
              ),
              const SizedBox(height: 10),
              Text(
                success
                    ? '${controller.connectedSsid} · ${controller.ipAddress ?? ''}'
                    : (controller.errorMessage ?? 'Something went wrong.'),
                textAlign: TextAlign.center,
                style: Theme.of(context)
                    .textTheme
                    .bodyLarge
                    ?.copyWith(color: scheme.onSurfaceVariant),
              ),
              const SizedBox(height: 44),
              if (!success)
                FilledButton(
                  key: const Key('retry_button'),
                  onPressed: controller.retry,
                  child: const Text('Try another network'),
                ),
              if (!success) const SizedBox(height: 12),
              OutlinedButton(
                key: const Key('startover_button'),
                style: OutlinedButton.styleFrom(
                  minimumSize: const Size.fromHeight(56),
                  shape: const StadiumBorder(),
                ),
                onPressed: controller.startOver,
                child: Text(success ? 'Provision another device' : 'Start over'),
              ),
            ]
                .animate(interval: 70.ms)
                .fadeIn(duration: 400.ms)
                .slideY(begin: 0.1, end: 0, curve: Curves.easeOutCubic),
          ),
        ),
      ),
    );
  }
}
