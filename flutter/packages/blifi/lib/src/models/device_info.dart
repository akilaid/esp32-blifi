/// Device metadata read over the plaintext Device-Info characteristic.
library;

/// Non-sensitive device metadata (docs/protocol-spec.md §6.1), read before the
/// handshake so the app can adapt (e.g. only prompt for a PoP when required).
class BlifiDeviceInfo {
  /// Creates a [BlifiDeviceInfo].
  const BlifiDeviceInfo({
    required this.protocolVersion,
    required this.firmwareVersion,
    required this.name,
    required this.state,
    required this.popRequired,
  });

  /// The blifi protocol version the device speaks.
  final int protocolVersion;

  /// The device firmware version string.
  final String firmwareVersion;

  /// The device's advertised name.
  final String name;

  /// The device's provisioning state (`unprovisioned`, `connected`, …).
  final String state;

  /// Whether a Proof-of-Possession is required to provision this device.
  final bool popRequired;

  @override
  String toString() => 'BlifiDeviceInfo($name, fw=$firmwareVersion, pop=$popRequired)';
}
