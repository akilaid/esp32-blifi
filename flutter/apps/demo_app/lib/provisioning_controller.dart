import 'dart:async';

import 'package:blifi/blifi.dart';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';

/// Steps of the provisioning flow the UI renders.
enum ProvisioningStage {
  permissions,
  home,
  qrScan,
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
  String connectingLabel = '';
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

  /// Request BLE runtime permissions, then go to the home screen.
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
      errorMessage = null;
      _set(ProvisioningStage.home);
    } else {
      errorMessage = 'Bluetooth permission is required to continue.';
      _set(ProvisioningStage.permissions);
    }
    return ok;
  }

  /// Open the QR scanner (requests camera permission first).
  Future<bool> openQrScanner() async {
    final cam = await Permission.camera.request();
    if (!cam.isGranted) {
      errorMessage = 'Camera permission is required to scan a QR code.';
      notifyListeners();
      return false;
    }
    errorMessage = null;
    _set(ProvisioningStage.qrScan);
    return true;
  }

  /// Start (or restart) scanning for blifi devices (manual selection path).
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

  /// QR path: find the named device, then connect with its PoP.
  Future<void> connectViaQr(String name, String? pop) async {
    errorMessage = null;
    connectingLabel = 'Looking for $name…';
    _set(ProvisioningStage.connecting);
    BlifiDevice? found;
    try {
      await for (final d in _blifi.scanForDevices(timeout: const Duration(seconds: 15))) {
        if (d.name == name) {
          found = d;
          break;
        }
      }
    } catch (e) {
      errorMessage = _messageFor(e);
      _set(ProvisioningStage.failure);
      return;
    }
    if (found == null) {
      errorMessage = 'Couldn’t find “$name” nearby. Make sure it’s powered on.';
      _set(ProvisioningStage.failure);
      return;
    }
    await _doConnect(found, pop);
  }

  /// Manual path: connect to a tapped device with the entered PoP.
  Future<void> connect(BlifiDevice target, String pop) async {
    await _scanSub?.cancel();
    await _doConnect(target, pop);
  }

  Future<void> _doConnect(BlifiDevice target, String? pop) async {
    device = target;
    errorMessage = null;
    connectingLabel = 'Establishing a secure session…';
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
      connectingLabel = 'Fetching nearby Wi-Fi networks…';
      notifyListeners();
      final nets = await session.scanWifiNetworks();
      networks = nets..sort((a, b) => b.rssi.compareTo(a.rssi));
      _set(ProvisioningStage.wifiList);
    } on AuthenticationException {
      errorMessage = 'Wrong Proof-of-Possession — double-check and try again.';
      _set(ProvisioningStage.failure);
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

  /// From a failure, go back to the network list (if the session is alive).
  void retry() {
    errorMessage = null;
    if (_session != null) {
      _set(ProvisioningStage.wifiList);
    } else {
      startOver();
    }
  }

  /// Disconnect and return to the home screen.
  Future<void> startOver() async {
    await _statusSub?.cancel();
    _statusSub = null;
    await _scanSub?.cancel();
    _scanSub = null;
    await _session?.disconnect();
    _session = null;
    networks = const [];
    device = null;
    connectedSsid = null;
    status = null;
    errorMessage = null;
    _set(ProvisioningStage.home);
  }

  String _errorText(ProvisioningState s) => switch (s) {
        ProvisioningState.wifiAuthError => 'Wrong Wi-Fi password.',
        ProvisioningState.wifiNotFound => 'That network wasn’t found.',
        ProvisioningState.wifiTimeout => 'The connection timed out.',
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
