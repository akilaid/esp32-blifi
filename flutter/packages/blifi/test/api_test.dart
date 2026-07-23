import 'package:flutter_test/flutter_test.dart';

import 'package:blifi/blifi.dart';

void main() {
  group('WifiAuthMode', () {
    test('maps wire codes', () {
      expect(WifiAuthMode.fromCode(0), WifiAuthMode.open);
      expect(WifiAuthMode.fromCode(3), WifiAuthMode.wpa2Psk);
      expect(WifiAuthMode.fromCode(6), WifiAuthMode.wpa3Psk);
      expect(WifiAuthMode.fromCode(999), WifiAuthMode.unknown);
    });
    test('requiresPassword', () {
      expect(WifiAuthMode.open.requiresPassword, isFalse);
      expect(WifiAuthMode.wpa2Psk.requiresPassword, isTrue);
    });
  });

  group('ProvisioningState', () {
    test('maps codes and flags errors', () {
      expect(ProvisioningState.fromCode(0x12), ProvisioningState.wifiConnected);
      expect(ProvisioningState.wifiConnected.isError, isFalse);
      expect(ProvisioningState.fromCode(0x22), ProvisioningState.wifiAuthError);
      expect(ProvisioningState.wifiAuthError.isError, isTrue);
      expect(ProvisioningState.fromCode(0xff), ProvisioningState.internalError);
    });
  });

  group('WifiNetwork.fromJson', () {
    test('parses a scan entry', () {
      final n = WifiNetwork.fromJson(const {
        'ssid': 'Home',
        'rssi': -47,
        'auth': 3,
        'channel': 6,
        'hidden': false,
      });
      expect(n.ssid, 'Home');
      expect(n.authMode, WifiAuthMode.wpa2Psk);
      expect(n.rssi, -47);
    });
  });

  group('exceptions', () {
    test('all extend BlifiException', () {
      expect(const AuthenticationException(), isA<BlifiException>());
      expect(const BleConnectionException(), isA<BlifiException>());
      expect(const BleUnavailableException(), isA<BlifiException>());
      expect(const ProvisioningTimeoutException(), isA<BlifiException>());
      expect(const WifiConnectionException(ProvisioningState.wifiNotFound), isA<BlifiException>());
    });
    test('WifiConnectionException carries the state', () {
      const e = WifiConnectionException(ProvisioningState.wifiAuthError, 'bad password');
      expect(e.state, ProvisioningState.wifiAuthError);
      expect(e.message, 'bad password');
    });
  });

  test('ProvisioningStatus reports errors and formats', () {
    const ok = ProvisioningStatus(ProvisioningState.wifiConnected, ipAddress: '10.0.0.5');
    expect(ok.isError, isFalse);
    expect(ok.toString(), contains('10.0.0.5'));
    const err = ProvisioningStatus(ProvisioningState.authFailed);
    expect(err.isError, isTrue);
  });
}
