/// The public blifi provisioning API - a thin, typed wrapper over the internal
/// BLE/crypto/protocol layers.
library;

import 'dart:async';

import 'package:flutter/foundation.dart' show visibleForTesting;
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble/provisioning_session.dart';
import 'protocol/constants.dart';
import 'protocol/messages.dart';
import 'models/blifi_device.dart';
import 'models/device_info.dart';
import 'models/exceptions.dart';
import 'models/provisioning_status.dart';
import 'models/wifi_network.dart';

/// Entry point for BLE-based Wi-Fi provisioning of a blifi device.
///
/// ```dart
/// final blifi = BlifiProvisioning();
/// final device = await blifi.scanForDevices().first;
/// final session = await blifi.connect(device, proofOfPossession: 'K7M2QP9X');
/// final networks = await session.scanWifiNetworks();
/// await session.sendCredentials('HomeWiFi', 'password');
/// session.statusStream.listen(print);
/// ```
class BlifiProvisioning {
  /// Creates a [BlifiProvisioning] instance (stateless; reuse freely).
  const BlifiProvisioning();

  /// Emits blifi devices as they are discovered, until [timeout] elapses or the
  /// subscription is cancelled.
  ///
  /// Throws [BleUnavailableException] if Bluetooth is unsupported or off.
  Stream<BlifiDevice> scanForDevices({
    Duration timeout = const Duration(seconds: 15),
  }) {
    final controller = StreamController<BlifiDevice>();
    final seen = <String>{};
    StreamSubscription<List<ScanResult>>? resultsSub;
    StreamSubscription<bool>? scanningSub;

    Future<void> start() async {
      try {
        await _ensureAdapterReady();
        final serviceGuid = Guid(BlifiUuids.service);
        resultsSub = FlutterBluePlus.onScanResults.listen((results) {
          for (final r in results) {
            final name = r.advertisementData.advName;
            final isBlifi = name.toLowerCase().startsWith('blifi') ||
                r.advertisementData.serviceUuids.contains(serviceGuid);
            if (isBlifi && seen.add(r.device.remoteId.str)) {
              controller.add(
                BlifiDevice(id: r.device.remoteId.str, name: name, rssi: r.rssi),
              );
            }
          }
        });
        // isScanning emits its current value (false) on listen, before the
        // scan starts - only close once scanning has actually gone true→false.
        var scanStarted = false;
        scanningSub = FlutterBluePlus.isScanning.listen((scanning) {
          if (scanning) {
            scanStarted = true;
          } else if (scanStarted && !controller.isClosed) {
            controller.close();
          }
        });
        await FlutterBluePlus.startScan(timeout: timeout);
      } catch (e) {
        if (!controller.isClosed) {
          controller.addError(_mapError(e));
          await controller.close();
        }
      }
    }

    controller
      ..onListen = start
      ..onCancel = () async {
        await resultsSub?.cancel();
        await scanningSub?.cancel();
        try {
          await FlutterBluePlus.stopScan();
        } catch (_) {}
      };
    return controller.stream;
  }

  /// Connect to [device] and complete the secure handshake.
  ///
  /// [proofOfPossession] must match the device's PoP unless it runs in no-PoP
  /// mode (see [BlifiDeviceInfo.popRequired]). Throws [AuthenticationException]
  /// on a wrong PoP, [BleConnectionException] on a BLE failure, or
  /// [ProvisioningTimeoutException] if the handshake stalls.
  Future<BlifiProvisioningSession> connect(
    BlifiDevice device, {
    String? proofOfPossession,
  }) async {
    await _ensureAdapterReady();
    final bleDevice = BluetoothDevice.fromId(device.id);
    try {
      final session =
          await ProvisioningSession.connect(bleDevice, pop: proofOfPossession);
      return BlifiProvisioningSession._(session);
    } catch (e) {
      throw _mapError(e);
    }
  }

  Future<void> _ensureAdapterReady() async {
    if (!await FlutterBluePlus.isSupported) {
      throw const BleUnavailableException('Bluetooth LE is not supported');
    }
    final state = await FlutterBluePlus.adapterState.first;
    if (state != BluetoothAdapterState.on) {
      throw const BleUnavailableException('Bluetooth is turned off');
    }
  }
}

/// An established, authenticated provisioning session with a device.
class BlifiProvisioningSession {
  BlifiProvisioningSession._(this._session) {
    // An internal listener so a completeError below is never an unhandled async
    // error when the app doesn't call awaitProvisioned(). Real awaiters still get
    // their own delivery.
    _provisioned.future.then((_) {}, onError: (_) {});
    _sub = _session.statusStream.listen(
      (msg) {
        final status = _mapStatus(msg);
        if (status.state == ProvisioningState.wifiConnected) {
          _ipAddress = status.ipAddress;
          if (status.ipAddress != null && !_provisioned.isCompleted) {
            _provisioned.complete(status.ipAddress!);
          }
        }
        if (!_controller.isClosed) _controller.add(status);
      },
      onError: (Object e) {
        if (!_controller.isClosed) _controller.addError(_mapError(e));
      },
      onDone: () {
        // The link dropped. After wifiConnected this is the expected clean end
        // (e.g. the device tore BLE down after provisioning); before it, the
        // session ended prematurely - a failure.
        if (_ipAddress == null) {
          if (!_controller.isClosed) {
            _controller.addError(const BleConnectionException(
                'disconnected before provisioning completed'));
          }
          if (!_provisioned.isCompleted) {
            _provisioned.completeError(const BleConnectionException(
                'disconnected before provisioning completed'));
          }
        }
        if (!_controller.isClosed) _controller.close();
      },
    );
  }

  /// Test-only constructor that injects a fake [ProvisioningSessionApi].
  @visibleForTesting
  BlifiProvisioningSession.forTest(ProvisioningSessionApi session)
      : this._(session);

  final ProvisioningSessionApi _session;
  final _controller = StreamController<ProvisioningStatus>.broadcast();
  final _provisioned = Completer<String>();
  StreamSubscription<StatusMessage>? _sub;
  String? _ipAddress;

  /// Metadata read from the device before the handshake.
  BlifiDeviceInfo get deviceInfo => BlifiDeviceInfo(
        protocolVersion: _session.deviceInfo.proto,
        firmwareVersion: _session.deviceInfo.firmware,
        name: _session.deviceInfo.name,
        state: _session.deviceInfo.state,
        popRequired: _session.deviceInfo.popRequired,
      );

  /// The device's IP address once connected, otherwise null.
  ///
  /// Cached from the last `wifiConnected` status, so it stays readable after the
  /// device disconnects. This is the last chance to learn the address: once the
  /// BLE link is gone the device is reachable only over the network.
  String? get ipAddress => _ipAddress;

  /// Live provisioning status updates (after [sendCredentials]).
  ///
  /// Completes normally (`onDone`) when the link drops after a successful
  /// `wifiConnected`; if the link drops before success it emits a
  /// [BleConnectionException] first.
  Stream<ProvisioningStatus> get statusStream => _controller.stream;

  /// Resolves to the device IP when provisioning succeeds, or throws
  /// [BleConnectionException] if the link drops before that.
  ///
  /// A convenience over racing [statusStream] against a disconnect. Note it does
  /// NOT complete on error *statuses* (e.g. a wrong Wi-Fi password) - the device
  /// stays connected for a retry, so watch [statusStream] for those.
  Future<String> awaitProvisioned() => _provisioned.future;

  /// Ask the device to scan for nearby Wi-Fi networks (encrypted).
  Future<List<WifiNetwork>> scanWifiNetworks() async {
    try {
      return await _session.scanWifiNetworks();
    } catch (e) {
      throw _mapError(e);
    }
  }

  /// Send Wi-Fi credentials to the device; progress arrives on [statusStream].
  ///
  /// [bssid] and [channel] are optional hints that speed up connection.
  Future<void> sendCredentials(
    String ssid,
    String password, {
    String? bssid,
    int? channel,
  }) async {
    try {
      await _session.sendCredentials(ssid, password, bssid: bssid, channel: channel);
    } catch (e) {
      throw _mapError(e);
    }
  }

  /// Disconnect and release resources. Idempotent - safe to call repeatedly
  /// (e.g. from `dispose()`) and after the link has already dropped.
  Future<void> disconnect() async {
    if (!_provisioned.isCompleted) {
      _provisioned.completeError(
          const BleConnectionException('session disconnected'));
    }
    await _sub?.cancel();
    if (!_controller.isClosed) await _controller.close();
    await _session.disconnect();
  }
}

// --- shared mapping ---

ProvisioningStatus _mapStatus(StatusMessage msg) => ProvisioningStatus(
      ProvisioningState.fromCode(msg.status.code),
      ipAddress: msg.ip,
      message: msg.detail,
    );

BlifiException _mapError(Object e) {
  if (e is BlifiException) return e;
  if (e is BlifiAuthException) return AuthenticationException(e.message);
  if (e is TimeoutException) return const ProvisioningTimeoutException();
  if (e is FlutterBluePlusException) return BleConnectionException(e.toString());
  return BleConnectionException(e.toString());
}
