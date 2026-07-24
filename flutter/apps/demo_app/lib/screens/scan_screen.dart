import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

import '../provisioning_controller.dart';

/// Manual device discovery: a live, animated list; tap to enter the PoP.
class ScanScreen extends StatelessWidget {
  const ScanScreen(this.controller, {super.key});

  final ProvisioningController controller;

  Future<void> _onTap(BuildContext context, BlifiDevice device) async {
    final pop = await showModalBottomSheet<String>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (_) => _PopSheet(device.name),
    );
    if (pop == null) return;
    await controller.connect(device, pop);
  }

  @override
  Widget build(BuildContext context) {
    final scanning = controller.stage == ProvisioningStage.scanning && !controller.scanComplete;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Nearby devices'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back_rounded),
          onPressed: controller.startOver,
        ),
      ),
      floatingActionButton: FloatingActionButton.extended(
        key: const Key('scan_button'),
        onPressed: controller.startScan,
        icon: const Icon(Icons.refresh_rounded),
        label: const Text('Rescan'),
      ),
      body: Column(
        children: [
          if (scanning) const LinearProgressIndicator(),
          Expanded(
            child: controller.devices.isEmpty
                ? _EmptyState(scanning: scanning)
                : ListView.builder(
                    padding: const EdgeInsets.fromLTRB(12, 12, 12, 96),
                    itemCount: controller.devices.length,
                    itemBuilder: (context, i) {
                      final d = controller.devices[i];
                      return Card(
                        margin: const EdgeInsets.symmetric(vertical: 6, horizontal: 4),
                        child: ListTile(
                          key: Key('device_${d.id}'),
                          contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 6),
                          leading: CircleAvatar(
                            backgroundColor: Theme.of(context).colorScheme.primaryContainer,
                            child: Icon(Icons.developer_board_rounded,
                                color: Theme.of(context).colorScheme.onPrimaryContainer),
                          ),
                          title: Text(d.name, style: const TextStyle(fontWeight: FontWeight.w600)),
                          subtitle: Text('${d.rssi} dBm'),
                          trailing: const Icon(Icons.chevron_right_rounded),
                          onTap: () => _onTap(context, d),
                        ),
                      )
                          .animate(key: ValueKey(d.id))
                          .fadeIn(duration: 350.ms)
                          .slideX(begin: 0.12, end: 0, curve: Curves.easeOutCubic);
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class _EmptyState extends StatelessWidget {
  const _EmptyState({required this.scanning});
  final bool scanning;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: scanning
            ? [
                const SizedBox(width: 56, height: 56, child: CircularProgressIndicator()),
                const SizedBox(height: 24),
                Text('Scanning for devices…', style: theme.textTheme.titleMedium),
              ]
            : [
                Icon(Icons.bluetooth_disabled_rounded, size: 56, color: theme.colorScheme.outline),
                const SizedBox(height: 16),
                Text('No devices found', style: theme.textTheme.titleMedium),
                const SizedBox(height: 4),
                Text('Make sure your device is powered on, then Rescan.',
                    style: theme.textTheme.bodyMedium
                        ?.copyWith(color: theme.colorScheme.onSurfaceVariant)),
              ],
      ),
    );
  }
}

class _PopSheet extends StatefulWidget {
  const _PopSheet(this.deviceName);
  final String deviceName;
  @override
  State<_PopSheet> createState() => _PopSheetState();
}

class _PopSheetState extends State<_PopSheet> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bottom = MediaQuery.of(context).viewInsets.bottom;
    return Padding(
      padding: EdgeInsets.fromLTRB(24, 8, 24, 24 + bottom),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(widget.deviceName, style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 4),
          Text('Enter the Proof-of-Possession printed on the device.',
              style: Theme.of(context)
                  .textTheme
                  .bodyMedium
                  ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant)),
          const SizedBox(height: 20),
          TextField(
            key: const Key('pop_field'),
            controller: _controller,
            autofocus: true,
            textCapitalization: TextCapitalization.characters,
            decoration: const InputDecoration(
              labelText: 'Proof-of-Possession',
              hintText: 'e.g. K7M2QP9X',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 20),
          FilledButton(
            key: const Key('pop_ok'),
            onPressed: () => Navigator.pop(context, _controller.text.trim()),
            child: const Text('Connect'),
          ),
        ],
      ),
    );
  }
}
