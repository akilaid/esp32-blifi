import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';

import 'package:blifi/src/protocol/constants.dart';
import 'package:blifi/src/protocol/frame.dart';
import 'package:blifi/src/protocol/messages.dart';

void main() {
  test('frame chunk then reassemble (multi)', () {
    final payload = Uint8List.fromList(List.generate(200, (i) => (i * 7 + 1) & 0xff));
    final frames = buildFrames(MsgType.scanResponse, payload, 40);
    expect(frames.length, greaterThan(1));

    final r = Reassembler();
    ReasmMessage? msg;
    for (final f in frames) {
      msg = r.feed(f) ?? msg;
    }
    expect(msg, isNotNull);
    expect(msg!.msgType, MsgType.scanResponse);
    expect(msg.payload, equals(payload));
  });

  test('frame single chunk', () {
    final payload = Uint8List.fromList([1, 2, 3, 4, 5]);
    final frames = buildFrames(MsgType.hsPubkey, payload, 100);
    expect(frames.length, 1);
    final msg = Reassembler().feed(frames.first);
    expect(msg!.payload, equals(payload));
  });

  test('bad version throws', () {
    final bad = Uint8List.fromList([0x99, 0x02, 0, 0, 0, 1, 0, 1, 0xaa]);
    expect(() => Reassembler().feed(bad), throwsA(isA<FormatException>()));
  });

  test('scan response decode', () {
    const json =
        '{"networks":[{"ssid":"Home","rssi":-47,"auth":3,"channel":6,"hidden":false}]}';
    final nets = decodeScanResponse(Uint8List.fromList(utf8.encode(json)));
    expect(nets.length, 1);
    expect(nets.first.ssid, 'Home');
    expect(nets.first.authMode, 3);
    expect(nets.first.channel, 6);
    expect(nets.first.hidden, isFalse);
  });

  test('credentials encode matches firmware wire form', () {
    expect(utf8.decode(encodeCredentials('Net', 'pw')), '{"ssid":"Net","password":"pw"}');
    final withHint = utf8.decode(encodeCredentials('N', 'p', bssid: 'aa:bb:cc:dd:ee:ff', channel: 6));
    expect(withHint.contains('"bssid":"aa:bb:cc:dd:ee:ff"'), isTrue);
  });

  test('device info decode', () {
    const json =
        '{"proto":1,"fw":"1.0.0","name":"blifi-A1B2","state":"unprovisioned","pop_required":true}';
    final di = DeviceInfo.fromBytes(Uint8List.fromList(utf8.encode(json)));
    expect(di.proto, 1);
    expect(di.name, 'blifi-A1B2');
    expect(di.popRequired, isTrue);
  });

  test('status decode', () {
    const json = '{"code":18,"detail":"got ip","ip":"192.168.1.42"}';
    final st = StatusMessage.fromBytes(Uint8List.fromList(utf8.encode(json)));
    expect(st.status, ProvStatus.wifiConnected);
    expect(st.ip, '192.168.1.42');
  });
}
