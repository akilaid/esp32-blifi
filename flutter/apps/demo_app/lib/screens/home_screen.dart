import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

import '../provisioning_controller.dart';

/// Choose how to reach a device: scan a QR code (primary) or browse manually.
class HomeScreen extends StatelessWidget {
  const HomeScreen(this.controller, {super.key});

  final ProvisioningController controller;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      appBar: AppBar(title: const Text('blifi')),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(20, 8, 20, 24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              const SizedBox(height: 8),
              Text('How would you like to connect?',
                  style: Theme.of(context).textTheme.titleLarge),
              const SizedBox(height: 20),
              _OptionCard(
                key: const Key('option_qr'),
                icon: Icons.qr_code_scanner_rounded,
                title: 'Scan QR code',
                subtitle: 'Point at the code on your device to connect instantly.',
                filled: true,
                onTap: controller.openQrScanner,
              ),
              const SizedBox(height: 16),
              _OptionCard(
                key: const Key('option_manual'),
                icon: Icons.bluetooth_searching_rounded,
                title: 'Find devices',
                subtitle: 'Browse nearby devices and enter the code manually.',
                filled: false,
                onTap: controller.startScan,
              ),
              const Spacer(),
              if (controller.errorMessage != null)
                Text(controller.errorMessage!,
                    textAlign: TextAlign.center, style: TextStyle(color: scheme.error)),
            ]
                .animate(interval: 90.ms)
                .fadeIn(duration: 450.ms)
                .slideY(begin: 0.1, end: 0, curve: Curves.easeOutCubic),
          ),
        ),
      ),
    );
  }
}

class _OptionCard extends StatelessWidget {
  const _OptionCard({
    super.key,
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.filled,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final bool filled;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final bg = filled ? scheme.primaryContainer : scheme.surfaceContainerHighest;
    final fg = filled ? scheme.onPrimaryContainer : scheme.onSurface;
    return Card(
      color: bg,
      elevation: filled ? 2 : 0,
      child: InkWell(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: Row(
            children: [
              Icon(icon, size: 40, color: fg),
              const SizedBox(width: 18),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: Theme.of(context).textTheme.titleLarge?.copyWith(color: fg)),
                    const SizedBox(height: 4),
                    Text(subtitle,
                        style: Theme.of(context)
                            .textTheme
                            .bodyMedium
                            ?.copyWith(color: fg.withValues(alpha: 0.8))),
                  ],
                ),
              ),
              Icon(Icons.chevron_right_rounded, color: fg),
            ],
          ),
        ),
      ),
    );
  }
}
