/// Session crypto (docs/security.md §4-§6), the app-side mirror of the firmware.
/// X25519 + HKDF-SHA256 + AES-256-GCM + HMAC-SHA256 via package:cryptography.
///
/// App role: transmits on k_app2dev, receives on k_dev2app (opposite of the
/// firmware device role).
library;

// Internal implementation; the documented public API lives in lib/blifi.dart.
// ignore_for_file: public_member_api_docs
import 'dart:convert';
import 'dart:typed_data';

import 'package:cryptography/cryptography.dart';

import '../protocol/constants.dart';

const int _keyLen = 32;
const int _tagLen = 16;
const int _counterLen = 4;
const int confirmLen = 16;

class ReplayException implements Exception {
  const ReplayException();
  @override
  String toString() => 'ReplayException: counter not increasing';
}

Uint8List _concat(List<List<int>> parts) {
  final b = BytesBuilder();
  for (final p in parts) {
    b.add(p);
  }
  return b.toBytes();
}

Uint8List _counterBytes(int counter) {
  final b = Uint8List(_counterLen);
  ByteData.view(b.buffer).setUint32(0, counter, Endian.big);
  return b;
}

/// One provisioning session (app side).
class BlifiSession {
  BlifiSession._(this._keyPair, this.appPublicKey);

  static final _x25519 = X25519();
  static final _aes = AesGcm.with256bits(nonceLength: 12);
  static final _hmac = Hmac.sha256();
  static final _hkdf = Hkdf(hmac: Hmac.sha256(), outputLength: _keyLen);

  final SimpleKeyPair _keyPair;

  /// The app's 32-byte X25519 public key.
  final Uint8List appPublicKey;

  late Uint8List _devicePublicKey;
  late Uint8List _kAppToDev;
  late Uint8List _kDevToApp;
  late Uint8List _kConfirm;

  int _txCounter = 0;
  int _rxLast = 0;
  bool _rxSeen = false;
  bool established = false;

  /// Create a session, optionally from a fixed 32-byte [seed] (for tests).
  static Future<BlifiSession> create({Uint8List? seed}) async {
    final kp = seed != null
        ? await _x25519.newKeyPairFromSeed(seed)
        : await _x25519.newKeyPair();
    final pub = await kp.extractPublicKey();
    return BlifiSession._(kp, Uint8List.fromList(pub.bytes));
  }

  Future<Uint8List> _hkdf32(Uint8List ikm, Uint8List salt, String info) async {
    final key = await _hkdf.deriveKey(
      secretKey: SecretKey(ikm),
      nonce: salt,
      info: utf8.encode(info),
    );
    return Uint8List.fromList(await key.extractBytes());
  }

  /// Derive the session keys from the device's public key and the PoP
  /// (security.md §4). Pass an empty string for the no-PoP dev mode.
  Future<void> deriveKeys(Uint8List devicePublicKey, String pop) async {
    _devicePublicKey = Uint8List.fromList(devicePublicKey);

    final shared = await _x25519.sharedSecretKey(
      keyPair: _keyPair,
      remotePublicKey: SimplePublicKey(devicePublicKey, type: KeyPairType.x25519),
    );
    final ecdh = Uint8List.fromList(await shared.extractBytes());

    final salt = _concat([appPublicKey, _devicePublicKey]);
    final ikm = _concat([ecdh, utf8.encode(pop)]);

    _kAppToDev = await _hkdf32(ikm, salt, 'blifi/v1 key app->dev');
    _kDevToApp = await _hkdf32(ikm, salt, 'blifi/v1 key dev->app');
    _kConfirm = await _hkdf32(ikm, salt, 'blifi/v1 confirm');

    _txCounter = 0;
    _rxLast = 0;
    _rxSeen = false;
    established = true;
  }

  Future<Uint8List> _confirm(String label) async {
    final msg = _concat([utf8.encode(label), appPublicKey, _devicePublicKey]);
    final mac = await _hmac.calculateMac(msg, secretKey: SecretKey(_kConfirm));
    return Uint8List.fromList(mac.bytes.sublist(0, confirmLen));
  }

  /// The confirmation tag the app sends (security.md §5).
  Future<Uint8List> confirmApp() => _confirm('blifi/v1 confirm app');

  /// The confirmation tag the device is expected to return.
  Future<Uint8List> confirmDev() => _confirm('blifi/v1 confirm dev');

  Uint8List _nonce(int counter) {
    final n = Uint8List(12);
    ByteData.view(n.buffer).setUint32(8, counter, Endian.big);
    return n;
  }

  /// Encrypt an app→device message into `counter‖ciphertext‖tag` (spec §5).
  Future<Uint8List> encrypt(int msgType, Uint8List plaintext) async {
    final counter = _txCounter;
    final box = await _aes.encrypt(
      plaintext,
      secretKey: SecretKey(_kAppToDev),
      nonce: _nonce(counter),
      aad: [kProtocolVersion, msgType],
    );
    _txCounter++;
    return _concat([_counterBytes(counter), box.cipherText, box.mac.bytes]);
  }

  /// Decrypt a device→app record. Throws [ReplayException] on a stale counter
  /// or a [SecretBoxAuthenticationError] on tag failure.
  Future<Uint8List> decrypt(int msgType, Uint8List record) async {
    final counter = ByteData.view(record.buffer, record.offsetInBytes)
        .getUint32(0, Endian.big);
    if (_rxSeen && counter <= _rxLast) {
      throw const ReplayException();
    }
    final ct = record.sublist(_counterLen, record.length - _tagLen);
    final tag = record.sublist(record.length - _tagLen);
    final pt = await _aes.decrypt(
      SecretBox(ct, nonce: _nonce(counter), mac: Mac(tag)),
      secretKey: SecretKey(_kDevToApp),
      aad: [kProtocolVersion, msgType],
    );
    _rxLast = counter;
    _rxSeen = true;
    return Uint8List.fromList(pt);
  }
}
