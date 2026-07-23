/// Protocol constants mirroring docs/protocol-spec.md. MUST match the firmware.
library;

// Internal implementation; the documented public API lives in lib/blifi.dart.
// ignore_for_file: public_member_api_docs

/// Protocol version byte present in every frame header.
const int kProtocolVersion = 0x01;

/// GATT service and characteristic UUIDs (protocol-spec §2).
class BlifiUuids {
  static const String service = '6b1a0001-5f3e-4b7a-9c2d-1e8f7a4c9b20';
  static const String deviceInfo = '6b1a0002-5f3e-4b7a-9c2d-1e8f7a4c9b20';
  static const String handshake = '6b1a0003-5f3e-4b7a-9c2d-1e8f7a4c9b20';
  static const String scan = '6b1a0004-5f3e-4b7a-9c2d-1e8f7a4c9b20';
  static const String credentials = '6b1a0005-5f3e-4b7a-9c2d-1e8f7a4c9b20';
  static const String status = '6b1a0006-5f3e-4b7a-9c2d-1e8f7a4c9b20';
}

/// Message types (protocol-spec §4).
class MsgType {
  static const int deviceInfo = 0x01;
  static const int hsPubkey = 0x02;
  static const int hsConfirm = 0x03;
  static const int hsFail = 0x04;
  static const int scanRequest = 0x10;
  static const int scanResponse = 0x11;
  static const int credentials = 0x20;
  static const int status = 0x30;
}

/// Provisioning status / error codes (protocol-spec §7, mirrors blifi_status.h).
enum ProvStatus {
  idle(0x00),
  handshakeInProgress(0x01),
  handshakeOk(0x02),
  credentialsReceived(0x10),
  wifiConnecting(0x11),
  wifiConnected(0x12),
  authFailed(0x20),
  wifiNotFound(0x21),
  wifiAuthError(0x22),
  wifiTimeout(0x23),
  wifiDisconnected(0x24),
  provTimeout(0x25),
  invalidMessage(0x26),
  internalError(0x27);

  const ProvStatus(this.code);
  final int code;

  static ProvStatus fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => ProvStatus.internalError);
}
