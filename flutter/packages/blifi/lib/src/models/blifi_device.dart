/// A discoverable blifi device model (no BLE-plugin types leak here).
library;

/// A blifi device discovered during a scan. Pass it to
/// [BlifiProvisioning.connect](../blifi/BlifiProvisioning/connect.html).
class BlifiDevice {
  /// Creates a [BlifiDevice].
  const BlifiDevice({required this.id, required this.name, required this.rssi});

  /// The platform BLE identifier (MAC on Android, UUID on iOS). Stable enough
  /// to reconnect to within a session.
  final String id;

  /// The advertised name (e.g. `blifi-A1B2`).
  final String name;

  /// Advertised signal strength in dBm at discovery time.
  final int rssi;

  @override
  bool operator ==(Object other) => other is BlifiDevice && other.id == id;

  @override
  int get hashCode => id.hashCode;

  @override
  String toString() => 'BlifiDevice($name, $id, $rssi dBm)';
}
