import 'dart:async';

import 'package:blifi/blifi.dart';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';

/// Steps of the provisioning flow the UI renders.
enum ProvisioningStage {
  permissions,
  scanning,
  connecting,
  wifiList,
  provisioning,
  success,
  failure,
}

/// Drives the whole provisioning flow on top of the blifi public API. A plain
/// [ChangeNotifier] so the boilerplate carries no state-management dependency.
class ProvisioningController extends ChangeNotifier {
  ProvisioningController([BlifiProvisioning? blifi])
      : _blifi = blifi ?? const BlifiProvisioning();

  final BlifiProvisioning _blifi;

  ProvisioningStage stage = ProvisioningStage.permissions;
  final List<BlifiDevice> devices = [];
  List<WifiNetwork> networks = const [];
  BlifiDevice? device;
  String? connectedSsid;
  ProvisioningStatus? status;
  String? errorMessage;
  bool scanComplete = false;

  BlifiProvisioningSession? _session;
  StreamSubscription<BlifiDevice>? _scanSub;
  StreamSubscription<ProvisioningStatus>? _statusSub;

  void _set(ProvisioningStage s) {
    stage = s;
    notifyListeners();
  }

  /// Request BLE runtime permissions. Returns true if provisioning can proceed.
  Future<bool> requestPermissions() async {
    final result = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse, // Android 11 uses location for BLE scan
    ].request();
    final ok = (result[Permission.locationWhenInUse]?.isGranted ?? false) ||
        ((result[Permission.bluetoothScan]?.isGranted ?? false) &&
            (result[Permission.bluetoothConnect]?.isGranted ?? false));
    if (ok) {
      await startScan();
    } else {
      errorMessage = 'Bluetooth permissions are required.';
      _set(ProvisioningStage.permissions);
    }
    return ok;
  }

  /// Start (or restart) scanning for blifi devices.
  Future<void> startScan() async {
    await _scanSub?.cancel();
    devices.clear();
    errorMessage = null;
    scanComplete = false;
    _set(ProvisioningStage.scanning);
    _scanSub = _blifi.scanForDevices(timeout: const Duration(seconds: 12)).listen(
      (d) {
        if (!devices.any((e) => e.id == d.id)) {
          devices.add(d);
          notifyListeners();
        }
      },
      onError: (Object e) {
        errorMessage = _messageFor(e);
        notifyListeners();
      },
      onDone: () {
        scanComplete = true;
        notifyListeners();
      },
    );
  }

  /// Connect + handshake with [pop], then load the device's Wi-Fi networks.
  Future<void> connect(BlifiDevice target, String pop) async {
    device = target;
    errorMessage = null;
    await _scanSub?.cancel();
    _set(ProvisioningStage.connecting);
    try {
      final session = await _blifi.connect(target, proofOfPossession: pop);
      _session = session;
      _statusSub = session.statusStream.listen(
        _onStatus,
        onError: (Object e) {
          errorMessage = _messageFor(e);
          _set(ProvisioningStage.failure);
        },
      );
      final nets = await session.scanWifiNetworks();
      networks = nets..sort((a, b) => b.rssi.compareTo(a.rssi));
      _set(ProvisioningStage.wifiList);
    } on AuthenticationException {
      errorMessage = 'Wrong Proof-of-Possession — try again.';
      await startScan();
    } catch (e) {
      errorMessage = _messageFor(e);
      _set(ProvisioningStage.failure);
    }
  }

  /// Send credentials for [net]; progress arrives via [status].
  Future<void> sendCredentials(WifiNetwork net, String password) async {
    connectedSsid = net.ssid;
    status = null;
    errorMessage = null;
    _set(ProvisioningStage.provisioning);
    try {
      await _session!.sendCredentials(net.ssid, password);
    } catch (e) {
      errorMessage = _messageFor(e);
      _set(ProvisioningStage.failure);
    }
  }

  void _onStatus(ProvisioningStatus s) {
    status = s;
    if (s.state == ProvisioningState.wifiConnected) {
      _set(ProvisioningStage.success);
    } else if (s.isError) {
      errorMessage = _errorText(s.state);
      _set(ProvisioningStage.failure);
    } else {
      notifyListeners();
    }
  }

  /// IP address once connected.
  String? get ipAddress => _session?.ipAddress;

  /// From a failure, go back to the network list to try again.
  void retry() {
    errorMessage = null;
    if (_session != null) {
      _set(ProvisioningStage.wifiList);
    } else {
      startOver();
    }
  }

  /// Disconnect and return to scanning (the app-side "forget"/re-provision).
  Future<void> startOver() async {
    await _statusSub?.cancel();
    _statusSub = null;
    await _session?.disconnect();
    _session = null;
    networks = const [];
    device = null;
    connectedSsid = null;
    status = null;
    await startScan();
  }

  String _errorText(ProvisioningState s) => switch (s) {
        ProvisioningState.wifiAuthError => 'Wrong Wi-Fi password.',
        ProvisioningState.wifiNotFound => 'Network not found.',
        ProvisioningState.wifiTimeout => 'Connection timed out.',
        ProvisioningState.wifiDisconnected => 'Disconnected from the network.',
        _ => 'Provisioning failed (${s.name}).',
      };

  String _messageFor(Object e) => e is BlifiException ? e.message : e.toString();

  @override
  void dispose() {
    _scanSub?.cancel();
    _statusSub?.cancel();
    _session?.disconnect();
    super.dispose();
  }
}
