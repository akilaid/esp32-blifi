import 'package:blifi_demo/qr_payload.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('parses a valid payload', () {
    final p = QrPayload.tryParse('blifi://provision?name=blifi-2F04&pop=ABCD2345&v=1');
    expect(p, isNotNull);
    expect(p!.deviceName, 'blifi-2F04');
    expect(p.pop, 'ABCD2345');
  });

  test('PoP is optional', () {
    final p = QrPayload.tryParse('blifi://provision?name=blifi-2F04');
    expect(p, isNotNull);
    expect(p!.deviceName, 'blifi-2F04');
    expect(p.pop, isNull);
  });

  test('rejects non-blifi scheme and missing name', () {
    expect(QrPayload.tryParse('https://example.com?name=x'), isNull);
    expect(QrPayload.tryParse('blifi://provision?pop=ABCD2345'), isNull);
    expect(QrPayload.tryParse('not a uri at all'), isNull);
  });

  test('round-trips through toQrData', () {
    const original = QrPayload('blifi-2F04', 'ABCD2345');
    final parsed = QrPayload.tryParse(original.toQrData());
    expect(parsed!.deviceName, original.deviceName);
    expect(parsed.pop, original.pop);
  });
}
