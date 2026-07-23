/// JSON payload codecs (docs/protocol-spec.md §6). Wire format is JSON; MUST
/// match the firmware's fields. Encoding uses dart:convert.
library;

import 'dart:convert';
import 'dart:typed_data';

import 'constants.dart';

/// A Wi-Fi network from a scan response (§6.3).
class WifiNetwork {
  WifiNetwork({
    required this.ssid,
    required this.rssi,
    required this.authMode,
    required this.channel,
    required this.hidden,
  });

  final String ssid;
  final int rssi;
  final int authMode; // ESP-IDF wifi_auth_mode_t
  final int channel;
  final bool hidden;

  factory WifiNetwork.fromJson(Map<String, dynamic> j) => WifiNetwork(
        ssid: (j['ssid'] ?? '') as String,
        rssi: (j['rssi'] ?? 0) as int,
        authMode: (j['auth'] ?? 0) as int,
        channel: (j['channel'] ?? 0) as int,
        hidden: (j['hidden'] ?? false) as bool,
      );
}

/// Device-Info payload (§6.1).
class DeviceInfo {
  DeviceInfo({
    required this.proto,
    required this.firmware,
    required this.name,
    required this.state,
    required this.popRequired,
  });

  final int proto;
  final String firmware;
  final String name;
  final String state;
  final bool popRequired;

  factory DeviceInfo.fromBytes(Uint8List bytes) {
    final j = jsonDecode(utf8.decode(bytes)) as Map<String, dynamic>;
    return DeviceInfo(
      proto: (j['proto'] ?? 0) as int,
      firmware: (j['fw'] ?? '') as String,
      name: (j['name'] ?? '') as String,
      state: (j['state'] ?? '') as String,
      popRequired: (j['pop_required'] ?? true) as bool,
    );
  }
}

/// A decrypted STATUS message (§6.5).
class StatusMessage {
  StatusMessage({required this.status, this.detail, this.ip});
  final ProvStatus status;
  final String? detail;
  final String? ip;

  factory StatusMessage.fromBytes(Uint8List bytes) {
    final j = jsonDecode(utf8.decode(bytes)) as Map<String, dynamic>;
    return StatusMessage(
      status: ProvStatus.fromCode((j['code'] ?? 0x27) as int),
      detail: j['detail'] as String?,
      ip: j['ip'] as String?,
    );
  }
}

Uint8List _utf8Json(Object o) => Uint8List.fromList(utf8.encode(jsonEncode(o)));

/// Encode a SCAN_REQUEST payload (§6.2).
Uint8List encodeScanRequest({bool refresh = true}) =>
    _utf8Json({'refresh': refresh});

/// Encode a CREDENTIALS payload (§6.4).
Uint8List encodeCredentials(
  String ssid,
  String password, {
  String? bssid,
  int? channel,
}) {
  final m = <String, dynamic>{'ssid': ssid, 'password': password};
  if (bssid != null && channel != null) {
    m['bssid'] = bssid;
    m['channel'] = channel;
  }
  return _utf8Json(m);
}

/// Decode a SCAN_RESPONSE payload (§6.3).
List<WifiNetwork> decodeScanResponse(Uint8List bytes) {
  final j = jsonDecode(utf8.decode(bytes)) as Map<String, dynamic>;
  final nets = (j['networks'] as List? ?? const []);
  return nets
      .map((e) => WifiNetwork.fromJson(e as Map<String, dynamic>))
      .toList(growable: false);
}
