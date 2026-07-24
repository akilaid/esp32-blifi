/// A small **reference** QR payload format for provisioning: adapt it to
/// whatever your factory prints. Format (a versioned URI):
///
/// ```
/// blifi://provision?name=blifi-A1B2&pop=K7M2QP9X&v=1
/// ```
///
/// The `pop` is optional (omit it for no-PoP devices).
class QrPayload {
  const QrPayload(this.deviceName, this.pop);

  /// The advertised device name to connect to (e.g. `blifi-A1B2`).
  final String deviceName;

  /// The Proof-of-Possession, or null if the device runs in no-PoP mode.
  final String? pop;

  /// Parse a scanned string; returns null if it isn't a valid blifi payload.
  static QrPayload? tryParse(String data) {
    final uri = Uri.tryParse(data.trim());
    if (uri == null || uri.scheme.toLowerCase() != 'blifi') return null;
    final name = uri.queryParameters['name'];
    if (name == null || name.isEmpty) return null;
    final pop = uri.queryParameters['pop'];
    final hasPop = pop != null && pop.isNotEmpty;
    return QrPayload(name, hasPop ? pop : null);
  }

  /// Encode this payload as a QR string (for generating stickers/testing).
  String toQrData() => Uri(
        scheme: 'blifi',
        host: 'provision',
        queryParameters: {
          'name': deviceName,
          'pop': ?pop,
          'v': '1',
        },
      ).toString();
}
