import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';

import '../provisioning_controller.dart';
import '../widgets/signal_icon.dart';

/// Lists the device's visible Wi-Fi networks and collects a password.
class WifiScreen extends StatelessWidget {
  const WifiScreen(this.controller, {super.key});

  final ProvisioningController controller;

  Future<void> _onTap(BuildContext context, WifiNetwork net) async {
    var password = '';
    if (net.authMode.requiresPassword) {
      final entered = await showDialog<String>(
        context: context,
        builder: (ctx) => _PasswordDialog(net.ssid),
      );
      if (entered == null) return;
      password = entered;
    }
    await controller.sendCredentials(net, password);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(controller.device?.name ?? 'Wi-Fi networks'),
        actions: [
          IconButton(
            tooltip: 'Disconnect',
            onPressed: controller.startOver,
            icon: const Icon(Icons.close),
          ),
        ],
      ),
      body: ListView(
        children: [
          for (final n in controller.networks)
            ListTile(
              key: Key('wifi_${n.ssid}'),
              leading: SignalIcon(n.rssi),
              title: Text(n.ssid.isEmpty ? '<hidden>' : n.ssid),
              subtitle: Text(n.authMode.name),
              trailing: n.authMode.requiresPassword
                  ? const Icon(Icons.lock_outline, size: 18)
                  : null,
              onTap: () => _onTap(context, n),
            ),
        ],
      ),
    );
  }
}

class _PasswordDialog extends StatefulWidget {
  const _PasswordDialog(this.ssid);
  final String ssid;
  @override
  State<_PasswordDialog> createState() => _PasswordDialogState();
}

class _PasswordDialogState extends State<_PasswordDialog> {
  final _controller = TextEditingController();
  bool _obscure = true;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(widget.ssid),
      content: TextField(
        key: const Key('password_field'),
        controller: _controller,
        autofocus: true,
        obscureText: _obscure,
        decoration: InputDecoration(
          labelText: 'Password',
          suffixIcon: IconButton(
            icon: Icon(_obscure ? Icons.visibility : Icons.visibility_off),
            onPressed: () => setState(() => _obscure = !_obscure),
          ),
        ),
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
        FilledButton(
          key: const Key('password_ok'),
          onPressed: () => Navigator.pop(context, _controller.text),
          child: const Text('Connect'),
        ),
      ],
    );
  }
}
