/// Wi-Fi network model returned by a device scan.
library;

/// Wi-Fi authentication mode, mirroring ESP-IDF `wifi_auth_mode_t`
/// (docs/protocol-spec.md §6.3).
enum WifiAuthMode {
  /// Open network (no password).
  open(0),

  /// WEP.
  wep(1),

  /// WPA-PSK.
  wpaPsk(2),

  /// WPA2-PSK.
  wpa2Psk(3),

  /// Mixed WPA/WPA2-PSK.
  wpaWpa2Psk(4),

  /// WPA2-Enterprise.
  wpa2Enterprise(5),

  /// WPA3-PSK.
  wpa3Psk(6),

  /// Mixed WPA2/WPA3-PSK.
  wpa2Wpa3Psk(7),

  /// WAPI-PSK.
  wapiPsk(8),

  /// Opportunistic Wireless Encryption.
  owe(9),

  /// Any mode this package does not model explicitly.
  unknown(-1);

  const WifiAuthMode(this.code);

  /// The ESP-IDF `wifi_auth_mode_t` value.
  final int code;

  /// True if joining requires a password.
  bool get requiresPassword => this != WifiAuthMode.open;

  /// Resolve a wire code (unknown values → [unknown]).
  static WifiAuthMode fromCode(int code) => values.firstWhere(
        (m) => m.code == code,
        orElse: () => WifiAuthMode.unknown,
      );
}

/// A Wi-Fi network discovered by the device during a scan.
class WifiNetwork {
  /// Creates a [WifiNetwork].
  const WifiNetwork({
    required this.ssid,
    required this.rssi,
    required this.authMode,
    required this.channel,
    required this.hidden,
  });

  /// Network name. Empty when [hidden].
  final String ssid;

  /// Signal strength in dBm (closer to 0 is stronger).
  final int rssi;

  /// Authentication mode.
  final WifiAuthMode authMode;

  /// Primary channel.
  final int channel;

  /// Whether the SSID is hidden.
  final bool hidden;

  /// Parse from the decrypted `SCAN_RESPONSE` JSON (§6.3).
  factory WifiNetwork.fromJson(Map<String, dynamic> j) => WifiNetwork(
        ssid: (j['ssid'] ?? '') as String,
        rssi: (j['rssi'] ?? 0) as int,
        authMode: WifiAuthMode.fromCode((j['auth'] ?? -1) as int),
        channel: (j['channel'] ?? 0) as int,
        hidden: (j['hidden'] ?? false) as bool,
      );

  @override
  String toString() => 'WifiNetwork($ssid, $rssi dBm, ${authMode.name})';
}
