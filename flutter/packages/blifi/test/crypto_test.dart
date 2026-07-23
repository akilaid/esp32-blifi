import 'dart:convert';
import 'dart:typed_data';

import 'package:cryptography/cryptography.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:blifi/src/crypto/blifi_crypto.dart';
import 'package:blifi/src/protocol/constants.dart';

Uint8List hx(String s) => Uint8List.fromList(
    [for (var i = 0; i < s.length; i += 2) int.parse(s.substring(i, i + 2), radix: 16)]);
String hex(List<int> b) => b.map((x) => x.toRadixString(16).padLeft(2, '0')).join();

// Vectors from the independent (OpenSSL) Python oracle — see scratchpad/blifi_oracle.py.
final appSeed = hx('000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f');
final devPub = hx('358072d6365880d1aeea329adf9121383851ed21a28e3b75e965d0d2cd166254');
const pop = 'TESTPOP1';
final appPub = hx('8f40c5adb68f25624ae5b214ea767a6ec94d829d3d7b5e1ad1ba6f3e2138285f');
final confirmApp = hx('cf04180e42da064e13ecd72d5b17b12a');
final confirmDev = hx('2adcf5eec748655765386a388ca4c8cd');
final recCreds = hx(
    '0000000073baf5041493ac5cc1c1f04e2cfb7600998c789d039bfec5529d55309c3b615ec82974dac33c5d32fc0630a26b50');
final recStatus = hx('00000000aa21b57e9417518dd99ecdac908b83229e5d57aef7f03450e6f246');

void main() {
  test('X25519 RFC 7748 vector', () async {
    final x = X25519();
    final scalar = hx('a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4');
    final u = hx('e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c');
    final want = 'c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552';
    final ss = await x.sharedSecretKey(
      keyPair: await x.newKeyPairFromSeed(scalar),
      remotePublicKey: SimplePublicKey(u, type: KeyPairType.x25519),
    );
    expect(hex(await ss.extractBytes()), want);
  });

  test('app public key derived from seed matches oracle', () async {
    final s = await BlifiSession.create(seed: appSeed);
    expect(hex(s.appPublicKey), hex(appPub));
  });

  test('blifi session vectors match firmware (via oracle)', () async {
    final s = await BlifiSession.create(seed: appSeed);
    await s.deriveKeys(devPub, pop);

    expect(hex(await s.confirmApp()), hex(confirmApp), reason: 'confirm_app');
    expect(hex(await s.confirmDev()), hex(confirmDev), reason: 'confirm_dev');

    final rec = await s.encrypt(
        MsgType.credentials, Uint8List.fromList(utf8.encode('{"ssid":"Net","password":"pw"}')));
    expect(hex(rec), hex(recCreds), reason: 'app->dev record');

    final pt = await s.decrypt(MsgType.status, recStatus);
    expect(utf8.decode(pt), '{"code":18}', reason: 'dev->app decrypt');
  });

  test('replay rejected', () async {
    final s = await BlifiSession.create(seed: appSeed);
    await s.deriveKeys(devPub, pop);
    await s.decrypt(MsgType.status, recStatus); // first accepted
    expect(() => s.decrypt(MsgType.status, recStatus), throwsA(isA<ReplayException>()));
  });

  test('AAD mismatch rejected', () async {
    final s = await BlifiSession.create(seed: appSeed);
    await s.deriveKeys(devPub, pop);
    // wrong msg_type in AAD → authentication failure
    expect(() => s.decrypt(MsgType.scanResponse, recStatus), throwsA(anything));
  });
}
