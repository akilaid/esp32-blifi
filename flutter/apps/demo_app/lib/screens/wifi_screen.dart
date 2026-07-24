import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

import '../provisioning_controller.dart';
import '../widgets/signal_icon.dart';

/// The device's visible Wi-Fi networks; collects a password on tap.
class WifiScreen extends StatelessWidget {
  const WifiScreen(this.controller, {super.key});

  final ProvisioningController controller;

  Future<void> _onTap(BuildContext context, WifiNetwork net) async {
    var password = '';
    if (net.authMode.requiresPassword) {
      final entered = await showModalBottomSheet<String>(
        context: context,
        isScrollControlled: true,
        showDragHandle: true,
        builder: (_) => _PasswordSheet(net.ssid),
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
        title: Text(controller.device?.name ?? 'Choose a network'),
        actions: [
          IconButton(
            tooltip: 'Disconnect',
            onPressed: controller.startOver,
            icon: const Icon(Icons.close_rounded),
          ),
        ],
      ),
      body: ListView.builder(
        padding: const EdgeInsets.fromLTRB(12, 8, 12, 24),
        itemCount: controller.networks.length,
        itemBuilder: (context, i) {
          final n = controller.networks[i];
          return Card(
            margin: const EdgeInsets.symmetric(vertical: 5, horizontal: 4),
            child: ListTile(
              key: Key('wifi_${n.ssid}'),
              contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 4),
              leading: SignalIcon(n.rssi),
              title: Text(n.ssid.isEmpty ? '‹hidden›' : n.ssid,
                  style: const TextStyle(fontWeight: FontWeight.w600)),
              subtitle: Text(_authLabel(n.authMode)),
              trailing: n.authMode.requiresPassword
                  ? const Icon(Icons.lock_outline_rounded, size: 18)
                  : const Icon(Icons.lock_open_rounded, size: 18),
              onTap: () => _onTap(context, n),
            ),
          )
              .animate(key: ValueKey(n.ssid))
              .fadeIn(duration: 300.ms)
              .slideY(begin: 0.08, end: 0, curve: Curves.easeOutCubic);
        },
      ),
    );
  }

  static String _authLabel(WifiAuthMode m) => switch (m) {
        WifiAuthMode.open => 'Open',
        WifiAuthMode.wep => 'WEP',
        WifiAuthMode.wpaPsk => 'WPA',
        WifiAuthMode.wpa2Psk => 'WPA2',
        WifiAuthMode.wpaWpa2Psk => 'WPA/WPA2',
        WifiAuthMode.wpa3Psk => 'WPA3',
        WifiAuthMode.wpa2Wpa3Psk => 'WPA2/WPA3',
        WifiAuthMode.wpa2Enterprise => 'WPA2-Enterprise',
        WifiAuthMode.wapiPsk => 'WAPI',
        WifiAuthMode.owe => 'Enhanced Open',
        WifiAuthMode.unknown => 'Secured',
      };
}

class _PasswordSheet extends StatefulWidget {
  const _PasswordSheet(this.ssid);
  final String ssid;
  @override
  State<_PasswordSheet> createState() => _PasswordSheetState();
}

class _PasswordSheetState extends State<_PasswordSheet> {
  final _controller = TextEditingController();
  bool _obscure = true;

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
          Text(widget.ssid, style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 16),
          TextField(
            key: const Key('password_field'),
            controller: _controller,
            autofocus: true,
            obscureText: _obscure,
            onSubmitted: (v) => Navigator.pop(context, v),
            decoration: InputDecoration(
              labelText: 'Password',
              border: const OutlineInputBorder(),
              suffixIcon: IconButton(
                icon: Icon(_obscure ? Icons.visibility_rounded : Icons.visibility_off_rounded),
                onPressed: () => setState(() => _obscure = !_obscure),
              ),
            ),
          ),
          const SizedBox(height: 20),
          FilledButton(
            key: const Key('password_ok'),
            onPressed: () => Navigator.pop(context, _controller.text),
            child: const Text('Connect'),
          ),
        ],
      ),
    );
  }
}
