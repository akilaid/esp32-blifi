/// Provisioning status types surfaced on the public API.
library;

/// A provisioning state / error code, mirroring the firmware `blifi_status_t`
/// (docs/protocol-spec.md §7). Codes `< 0x20` are progress states; `>= 0x20`
/// are errors ([isError]).
enum ProvisioningState {
  /// No provisioning in progress.
  idle(0x00),

  /// The secure handshake is underway.
  handshakeInProgress(0x01),

  /// The session key has been established.
  handshakeOk(0x02),

  /// The device decrypted and accepted the credentials.
  credentialsReceived(0x10),

  /// The device is attempting to join the Wi-Fi network.
  wifiConnecting(0x11),

  /// The device is online; an IP address is available.
  wifiConnected(0x12),

  /// Wrong Proof-of-Possession / confirmation mismatch.
  authFailed(0x20),

  /// The target SSID was not found.
  wifiNotFound(0x21),

  /// Wrong Wi-Fi password.
  wifiAuthError(0x22),

  /// Association or DHCP timed out.
  wifiTimeout(0x23),

  /// The connection dropped after joining.
  wifiDisconnected(0x24),

  /// The provisioning window elapsed.
  provTimeout(0x25),

  /// A malformed or failed-authentication message was received.
  invalidMessage(0x26),

  /// An unexpected device-side error.
  internalError(0x27);

  const ProvisioningState(this.code);

  /// The wire code (matches the firmware `blifi_status_t`).
  final int code;

  /// True for error states (`code >= 0x20`).
  bool get isError => code >= 0x20;

  /// Resolve a wire code to a [ProvisioningState] (unknown → [internalError]).
  static ProvisioningState fromCode(int code) => values.firstWhere(
        (s) => s.code == code,
        orElse: () => ProvisioningState.internalError,
      );
}

/// A provisioning status update emitted on
/// [BlifiProvisioningSession.statusStream](../blifi/BlifiProvisioningSession/statusStream.html).
class ProvisioningStatus {
  /// Creates a [ProvisioningStatus].
  const ProvisioningStatus(this.state, {this.ipAddress, this.message});

  /// The current state / error.
  final ProvisioningState state;

  /// The device's IP address, present when [state] is
  /// [ProvisioningState.wifiConnected].
  final String? ipAddress;

  /// An optional human-readable detail from the device.
  final String? message;

  /// True when [state] is an error.
  bool get isError => state.isError;

  @override
  String toString() =>
      'ProvisioningStatus(${state.name}${ipAddress != null ? ', ip=$ipAddress' : ''})';
}
