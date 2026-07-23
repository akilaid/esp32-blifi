/// BLE transport over flutter_blue_plus: connect, discover the blifi service,
/// frame/chunk writes, and reassemble inbound notifications per characteristic.
library;

import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/constants.dart';
import '../protocol/frame.dart';

/// Logical characteristics (protocol-spec §2).
enum BlifiChar { deviceInfo, handshake, scan, credentials, status }

String _uuidFor(BlifiChar c) => switch (c) {
      BlifiChar.deviceInfo => BlifiUuids.deviceInfo,
      BlifiChar.handshake => BlifiUuids.handshake,
      BlifiChar.scan => BlifiUuids.scan,
      BlifiChar.credentials => BlifiUuids.credentials,
      BlifiChar.status => BlifiUuids.status,
    };

/// A complete reassembled inbound message.
class TransportMessage {
  TransportMessage(this.characteristic, this.msgType, this.payload);
  final BlifiChar characteristic;
  final int msgType;
  final Uint8List payload;
}

class BlifiTransport {
  BlifiTransport(this.device);
  final BluetoothDevice device;

  final Map<BlifiChar, BluetoothCharacteristic> _chars = {};
  final Map<BlifiChar, Reassembler> _reasm = {
    for (final c in BlifiChar.values) c: Reassembler(),
  };
  final _messages = StreamController<TransportMessage>.broadcast();
  final _subs = <StreamSubscription<List<int>>>[];
  int _mtu = 23;

  Stream<TransportMessage> get messages => _messages.stream;

  /// Scan for a blifi device by advertised name (adv packet) or service UUID
  /// (scan response).
  static Future<BluetoothDevice?> findDevice(
      {Duration timeout = const Duration(seconds: 10)}) async {
    BluetoothDevice? found;
    final svc = Guid(BlifiUuids.service);
    final sub = FlutterBluePlus.scanResults.listen((results) {
      for (final r in results) {
        final name = r.advertisementData.advName.toLowerCase();
        if (name.startsWith('blifi') || r.advertisementData.serviceUuids.contains(svc)) {
          found ??= r.device;
        }
      }
    });
    await FlutterBluePlus.startScan(timeout: timeout);
    await FlutterBluePlus.isScanning.where((s) => !s).first;
    await sub.cancel();
    return found;
  }

  /// Connect, negotiate MTU, discover characteristics, and subscribe to
  /// notification channels.
  Future<void> connect() async {
    await device.connect(timeout: const Duration(seconds: 15));
    try {
      _mtu = await device.requestMtu(512);
    } catch (_) {
      _mtu = device.mtuNow;
    }

    final services = await device.discoverServices();
    final svc = services.firstWhere(
      (s) => s.uuid == Guid(BlifiUuids.service),
      orElse: () => throw StateError('blifi service not found'),
    );
    for (final c in BlifiChar.values) {
      final target = Guid(_uuidFor(c));
      for (final chr in svc.characteristics) {
        if (chr.uuid == target) {
          _chars[c] = chr;
          break;
        }
      }
    }

    for (final c in [BlifiChar.handshake, BlifiChar.scan, BlifiChar.status]) {
      final chr = _chars[c];
      if (chr == null) continue;
      await chr.setNotifyValue(true);
      _subs.add(chr.onValueReceived.listen((v) => _onFrame(c, v)));
    }
  }

  void _onFrame(BlifiChar c, List<int> value) {
    try {
      final msg = _reasm[c]!.feed(Uint8List.fromList(value));
      if (msg != null) {
        _messages.add(TransportMessage(c, msg.msgType, msg.payload));
      }
    } on FormatException {
      // drop malformed frame; reassembler already reset
    }
  }

  /// Read the plaintext Device-Info characteristic.
  Future<Uint8List> readDeviceInfo() async {
    final chr = _chars[BlifiChar.deviceInfo];
    if (chr == null) throw StateError('device-info characteristic missing');
    return Uint8List.fromList(await chr.read());
  }

  /// Frame [payload] and write it (chunked to the negotiated MTU) on [c].
  Future<void> send(BlifiChar c, int msgType, Uint8List payload) async {
    final chr = _chars[c];
    if (chr == null) throw StateError('characteristic $c missing');
    final maxChunk = _mtu - 3 - kFrameHeaderLen;
    for (final frame in buildFrames(msgType, payload, maxChunk < 1 ? 1 : maxChunk)) {
      await _writeWithRetry(chr, frame);
    }
  }

  // Android occasionally returns GATT_ERROR (133) on a write that races with a
  // just-received notification; the write did not land, so retrying is safe.
  Future<void> _writeWithRetry(BluetoothCharacteristic chr, Uint8List frame) async {
    for (var attempt = 0;; attempt++) {
      try {
        await chr.write(frame, withoutResponse: false);
        return;
      } catch (_) {
        if (attempt >= 3) rethrow;
        await Future.delayed(Duration(milliseconds: 200 * (attempt + 1)));
      }
    }
  }

  Future<void> dispose() async {
    for (final s in _subs) {
      await s.cancel();
    }
    await _messages.close();
    try {
      await device.disconnect();
    } catch (_) {}
  }
}
