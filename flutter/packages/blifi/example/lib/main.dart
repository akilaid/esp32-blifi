// Minimal example of the blifi public API: scan → connect → list Wi-Fi →
// send credentials → watch status. The full polished UI is a separate app;
// this stays intentionally small.
//
// Remember to add BLE permissions (see the package README) and request them at
// runtime before scanning.
import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';

void main() => runApp(const BlifiExampleApp());

class BlifiExampleApp extends StatelessWidget {
  const BlifiExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'blifi example',
      theme: ThemeData(colorSchemeSeed: Colors.blue, useMaterial3: true),
      home: const _HomePage(),
    );
  }
}

class _HomePage extends StatefulWidget {
  const _HomePage();
  @override
  State<_HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<_HomePage> {
  final _blifi = const BlifiProvisioning();
  final _devices = <BlifiDevice>[];
  BlifiProvisioningSession? _session;
  List<WifiNetwork> _networks = const [];
  String _log = '';

  void _append(String s) => setState(() => _log = '$s\n$_log');

  Future<void> _scan() async {
    setState(() => _devices.clear());
    try {
      await for (final d in _blifi.scanForDevices(timeout: const Duration(seconds: 8))) {
        setState(() => _devices.add(d));
      }
    } on BlifiException catch (e) {
      _append('scan error: ${e.message}');
    }
  }

  Future<void> _connect(BlifiDevice device) async {
    final pop = await _prompt('Proof-of-Possession', obscure: false);
    if (pop == null) return;
    try {
      final session = await _blifi.connect(device, proofOfPossession: pop);
      _append('connected: ${session.deviceInfo.name} (fw ${session.deviceInfo.firmwareVersion})');
      session.statusStream.listen(
        (s) => _append('status: ${s.state.name}${s.ipAddress != null ? ' @ ${s.ipAddress}' : ''}'),
        onError: (Object e) => _append('status error: $e'),
      );
      final networks = await session.scanWifiNetworks();
      setState(() {
        _session = session;
        _networks = networks;
      });
      _append('found ${networks.length} networks');
    } on AuthenticationException {
      _append('wrong Proof-of-Possession');
    } on BlifiException catch (e) {
      _append('connect error: ${e.message}');
    }
  }

  Future<void> _provision(WifiNetwork net) async {
    final session = _session;
    if (session == null) return;
    final pw = net.authMode.requiresPassword
        ? await _prompt('Password for ${net.ssid}', obscure: true)
        : '';
    if (pw == null) return;
    try {
      await session.sendCredentials(net.ssid, pw);
      _append('credentials sent for ${net.ssid}');
    } on BlifiException catch (e) {
      _append('send error: ${e.message}');
    }
  }

  Future<String?> _prompt(String title, {required bool obscure}) {
    final controller = TextEditingController();
    return showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(title),
        content: TextField(controller: controller, obscureText: obscure, autofocus: true),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          TextButton(onPressed: () => Navigator.pop(ctx, controller.text), child: const Text('OK')),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('blifi example')),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _scan,
        icon: const Icon(Icons.bluetooth_searching),
        label: const Text('Scan'),
      ),
      body: Column(
        children: [
          Expanded(
            child: ListView(
              children: [
                for (final d in _devices)
                  ListTile(
                    leading: const Icon(Icons.developer_board),
                    title: Text(d.name),
                    subtitle: Text('${d.id} · ${d.rssi} dBm'),
                    onTap: () => _connect(d),
                  ),
                for (final n in _networks)
                  ListTile(
                    leading: const Icon(Icons.wifi),
                    title: Text(n.ssid.isEmpty ? '<hidden>' : n.ssid),
                    subtitle: Text('${n.authMode.name} · ${n.rssi} dBm'),
                    onTap: () => _provision(n),
                  ),
              ],
            ),
          ),
          Container(
            width: double.infinity,
            height: 160,
            color: Colors.black87,
            padding: const EdgeInsets.all(8),
            child: SingleChildScrollView(
              child: Text(_log, style: const TextStyle(color: Colors.greenAccent, fontFamily: 'monospace', fontSize: 12)),
            ),
          ),
        ],
      ),
    );
  }
}
