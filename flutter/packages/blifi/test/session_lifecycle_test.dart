import 'dart:async';

import 'package:flutter_test/flutter_test.dart';

import 'package:blifi/blifi.dart';
import 'package:blifi/src/ble/provisioning_session.dart';
import 'package:blifi/src/protocol/constants.dart';
import 'package:blifi/src/protocol/messages.dart';

/// A fake session driven by the test: emit status messages and drop the link.
class _FakeSession implements ProvisioningSessionApi {
  final _status = StreamController<StatusMessage>.broadcast();
  bool disconnected = false;

  @override
  Stream<StatusMessage> get statusStream => _status.stream;

  @override
  DeviceInfo get deviceInfo => DeviceInfo(
        proto: 1,
        firmware: '0.0.0',
        name: 'blifi-TEST',
        state: 'provisioning',
        popRequired: true,
      );

  @override
  Future<List<WifiNetwork>> scanWifiNetworks() async => const [];

  @override
  Future<void> sendCredentials(String ssid, String password,
      {String? bssid, int? channel}) async {}

  @override
  Future<void> disconnect() async {
    disconnected = true;
    if (!_status.isClosed) await _status.close();
  }

  void emit(int code, {String? ip}) =>
      _status.add(StatusMessage(status: ProvStatus.fromCode(code), ip: ip));

  /// Simulate the BLE link dropping (firmware- or app-initiated).
  Future<void> drop() async {
    if (!_status.isClosed) await _status.close();
  }
}

// Status codes (protocol-spec §7).
const _wifiConnecting = 0x11;
const _wifiConnected = 0x12;

void main() {
  group('BlifiProvisioningSession lifecycle', () {
    test('disconnect after wifiConnected is a clean completion with the IP',
        () async {
      final fake = _FakeSession();
      final session = BlifiProvisioningSession.forTest(fake);

      final events = <ProvisioningStatus>[];
      final done = Completer<void>();
      session.statusStream.listen(events.add,
          onError: (Object e) => fail('unexpected error: $e'),
          onDone: done.complete);

      fake.emit(_wifiConnected, ip: '192.168.1.50');
      await Future<void>.delayed(Duration.zero);
      await fake.drop();
      await done.future;

      expect(session.ipAddress, '192.168.1.50'); // survives the disconnect
      expect(await session.awaitProvisioned(), '192.168.1.50');
      expect(events.last.state, ProvisioningState.wifiConnected);
    });

    test('disconnect before success surfaces an error and awaitProvisioned throws',
        () async {
      final fake = _FakeSession();
      final session = BlifiProvisioningSession.forTest(fake);

      Object? err;
      final done = Completer<void>();
      session.statusStream.listen((_) {},
          onError: (Object e) => err = e, onDone: done.complete);

      fake.emit(_wifiConnecting); // progress, not success
      await Future<void>.delayed(Duration.zero);
      await fake.drop();
      await done.future;

      expect(err, isA<BleConnectionException>());
      expect(session.ipAddress, isNull);
      await expectLater(
          session.awaitProvisioned(), throwsA(isA<BleConnectionException>()));
    });

    test('disconnect() is idempotent', () async {
      final fake = _FakeSession();
      final session = BlifiProvisioningSession.forTest(fake);

      await session.disconnect();
      await session.disconnect(); // must not throw
      expect(fake.disconnected, isTrue);
    });
  });
}
