/// Orchestrates the blifi provisioning flow (protocol-spec §8) on top of the
/// transport and session crypto: handshake, Wi-Fi scan, credential send, status.
/// The polished public API wraps this.
library;

// Internal implementation; the documented public API lives in lib/blifi.dart.
// ignore_for_file: public_member_api_docs
import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../crypto/blifi_crypto.dart';
import '../models/wifi_network.dart';
import '../protocol/constants.dart';
import '../protocol/messages.dart';
import 'ble_transport.dart';

class BlifiAuthException implements Exception {
  const BlifiAuthException([this.message = 'authentication failed']);
  final String message;
  @override
  String toString() => 'BlifiAuthException: $message';
}

class ProvisioningSession {
  ProvisioningSession._(this._transport, this._crypto, this.deviceInfo);

  final BlifiTransport _transport;
  final BlifiSession _crypto;
  final DeviceInfo deviceInfo;

  StreamSubscription<TransportMessage>? _sub;
  final _status = StreamController<StatusMessage>.broadcast();
  Completer<Uint8List>? _devicePubkey;
  Completer<void>? _confirmed;
  Completer<List<WifiNetwork>>? _scan;

  /// Live provisioning status updates from the device (after credentials).
  Stream<StatusMessage> get statusStream => _status.stream;

  static const _timeout = Duration(seconds: 12);

  /// Connect to [device] and run the full handshake with [pop]
  /// (null/empty ⇒ no-PoP mode). Throws [BlifiAuthException] on failure.
  static Future<ProvisioningSession> connect(BluetoothDevice device,
      {String? pop}) async {
    final transport = BlifiTransport(device);
    await transport.connect();

    final info = DeviceInfo.fromBytes(await transport.readDeviceInfo());
    final crypto = await BlifiSession.create();
    final session = ProvisioningSession._(transport, crypto, info);
    await session._handshake(pop ?? '');
    return session;
  }

  Future<void> _handshake(String pop) async {
    _devicePubkey = Completer<Uint8List>();
    _confirmed = Completer<void>();
    _sub = _transport.messages.listen(_onMessage);

    // 1. Exchange public keys.
    await _transport.send(BlifiChar.handshake, MsgType.hsPubkey, _crypto.appPublicKey);
    final devicePub = await _devicePubkey!.future.timeout(_timeout);
    await _crypto.deriveKeys(devicePub, pop);

    // 2. Confirm.
    await _transport.send(
        BlifiChar.handshake, MsgType.hsConfirm, await _crypto.confirmApp());
    await _confirmed!.future.timeout(_timeout);

    // Let the link settle before the first post-handshake write (Android BLE).
    await Future.delayed(const Duration(milliseconds: 400));
  }

  Future<void> _onMessage(TransportMessage msg) async {
    switch (msg.characteristic) {
      case BlifiChar.handshake:
        if (msg.msgType == MsgType.hsPubkey) {
          if (_devicePubkey?.isCompleted == false) _devicePubkey!.complete(msg.payload);
        } else if (msg.msgType == MsgType.hsConfirm) {
          final expected = await _crypto.confirmDev();
          final ok = _constEq(expected, msg.payload);
          if (_confirmed?.isCompleted == false) {
            ok
                ? _confirmed!.complete()
                : _confirmed!.completeError(const BlifiAuthException('device confirm mismatch'));
          }
        } else if (msg.msgType == MsgType.hsFail) {
          if (_confirmed?.isCompleted == false) {
            _confirmed!.completeError(const BlifiAuthException('device rejected PoP'));
          }
        }
        break;
      case BlifiChar.scan:
        if (msg.msgType == MsgType.scanResponse) {
          final pt = await _crypto.decrypt(MsgType.scanResponse, msg.payload);
          if (_scan?.isCompleted == false) _scan!.complete(decodeScanResponse(pt));
        }
        break;
      case BlifiChar.status:
        if (msg.msgType == MsgType.status) {
          final pt = await _crypto.decrypt(MsgType.status, msg.payload);
          _status.add(StatusMessage.fromBytes(pt));
        }
        break;
      default:
        break;
    }
  }

  /// Ask the device to scan for Wi-Fi networks (encrypted round-trip).
  Future<List<WifiNetwork>> scanWifiNetworks() async {
    _scan = Completer<List<WifiNetwork>>();
    final rec = await _crypto.encrypt(MsgType.scanRequest, encodeScanRequest());
    await _transport.send(BlifiChar.scan, MsgType.scanRequest, rec);
    return _scan!.future.timeout(_timeout);
  }

  /// Send Wi-Fi credentials; progress arrives on [statusStream].
  Future<void> sendCredentials(String ssid, String password,
      {String? bssid, int? channel}) async {
    final rec = await _crypto.encrypt(
        MsgType.credentials, encodeCredentials(ssid, password, bssid: bssid, channel: channel));
    await _transport.send(BlifiChar.credentials, MsgType.credentials, rec);
  }

  Future<void> disconnect() async {
    await _sub?.cancel();
    await _status.close();
    await _transport.dispose();
  }

  static bool _constEq(Uint8List a, Uint8List b) {
    if (a.length != b.length) return false;
    var d = 0;
    for (var i = 0; i < a.length; i++) {
      d |= a[i] ^ b[i];
    }
    return d == 0;
  }
}
