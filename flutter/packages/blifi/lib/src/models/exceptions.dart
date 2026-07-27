/// Typed exceptions for the blifi provisioning flow.
///
/// All exceptions extend [BlifiException] so callers can catch the whole family,
/// then branch on the concrete type instead of string-matching.
library;

import 'provisioning_status.dart';

/// Base class for all errors thrown by the blifi package.
abstract class BlifiException implements Exception {
  /// Creates a blifi exception with a human-readable [message].
  const BlifiException(this.message);

  /// A human-readable description (for logs/UI; not a stable machine key).
  final String message;

  @override
  String toString() => '$runtimeType: $message';
}

/// Bluetooth is unsupported on this device, or the adapter is off/unavailable.
class BleUnavailableException extends BlifiException {
  /// Creates a [BleUnavailableException].
  const BleUnavailableException([super.message = 'Bluetooth is not available']);
}

/// A BLE connection, discovery, or GATT I/O operation failed.
class BleConnectionException extends BlifiException {
  /// Creates a [BleConnectionException].
  const BleConnectionException([super.message = 'BLE connection failed']);
}

/// The secure handshake failed - a wrong Proof-of-Possession or a device
/// confirmation mismatch (protocol-spec `AUTH_FAILED`).
class AuthenticationException extends BlifiException {
  /// Creates an [AuthenticationException].
  const AuthenticationException([super.message = 'authentication failed']);
}

/// The device could not join the target Wi-Fi network. [state] carries the
/// specific reason (e.g. [ProvisioningState.wifiAuthError],
/// [ProvisioningState.wifiNotFound]).
class WifiConnectionException extends BlifiException {
  /// Creates a [WifiConnectionException] for the given [state].
  const WifiConnectionException(this.state, [String? message])
      : super(message ?? 'Wi-Fi connection failed');

  /// The specific failure reason from the device.
  final ProvisioningState state;
}

/// An operation (handshake, scan, or Wi-Fi connection) did not complete in time.
class ProvisioningTimeoutException extends BlifiException {
  /// Creates a [ProvisioningTimeoutException].
  const ProvisioningTimeoutException([super.message = 'operation timed out']);
}
