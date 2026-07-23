import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';

import '../provisioning_controller.dart';

/// Device discovery: lists blifi devices and prompts for the PoP on tap.
class ScanScreen extends StatelessWidget {
  const ScanScreen(this.controller, {super.key});

  final ProvisioningController controller;

  Future<void> _onTap(BuildContext context, BlifiDevice device) async {
    final pop = await showDialog<String>(
      context: context,
      builder: (ctx) => const _PopDialog(),
    );
    if (pop == null) return;
    await controller.connect(device, pop);
  }

  @override
  Widget build(BuildContext context) {
    final scanning = controller.stage == ProvisioningStage.scanning && !controller.scanComplete;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Select a device'),
        bottom: scanning
            ? const PreferredSize(
                preferredSize: Size.fromHeight(2), child: LinearProgressIndicator())
            : null,
      ),
      floatingActionButton: FloatingActionButton.extended(
        key: const Key('scan_button'),
        onPressed: controller.startScan,
        icon: const Icon(Icons.refresh),
        label: const Text('Scan'),
      ),
      body: Column(
        children: [
          if (controller.errorMessage != null)
            _Banner(controller.errorMessage!),
          Expanded(
            child: controller.devices.isEmpty
                ? Center(
                    child: Text(scanning ? 'Scanning…' : 'No devices found. Tap Scan.'))
                : ListView(
                    children: [
                      for (final d in controller.devices)
                        ListTile(
                          key: Key('device_${d.id}'),
                          leading: const Icon(Icons.developer_board),
                          title: Text(d.name),
                          subtitle: Text('${d.rssi} dBm'),
                          trailing: const Icon(Icons.chevron_right),
                          onTap: () => _onTap(context, d),
                        ),
                    ],
                  ),
          ),
        ],
      ),
    );
  }
}

class _PopDialog extends StatefulWidget {
  const _PopDialog();
  @override
  State<_PopDialog> createState() => _PopDialogState();
}

class _PopDialogState extends State<_PopDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Proof-of-Possession'),
      content: TextField(
        key: const Key('pop_field'),
        controller: _controller,
        autofocus: true,
        textCapitalization: TextCapitalization.characters,
        decoration: const InputDecoration(
          hintText: 'e.g. K7M2QP9X',
          helperText: 'Printed on the device serial output.',
        ),
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
        FilledButton(
          key: const Key('pop_ok'),
          onPressed: () => Navigator.pop(context, _controller.text.trim()),
          child: const Text('Connect'),
        ),
      ],
    );
  }
}

class _Banner extends StatelessWidget {
  const _Banner(this.message);
  final String message;
  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      color: Theme.of(context).colorScheme.errorContainer,
      padding: const EdgeInsets.all(12),
      child: Text(message, style: TextStyle(color: Theme.of(context).colorScheme.onErrorContainer)),
    );
  }
}
