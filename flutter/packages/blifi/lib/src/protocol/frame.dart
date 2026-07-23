/// Transport framing (docs/protocol-spec.md §3). Big-endian 8-byte header,
/// chunking, and per-characteristic reassembly. MUST match the firmware.
library;

// Internal implementation; the documented public API lives in lib/blifi.dart.
// ignore_for_file: public_member_api_docs

import 'dart:typed_data';

import 'constants.dart';

const int kFrameHeaderLen = 8;
const int kMaxMessageLen = 65535;

/// Split [payload] into frames of at most [maxChunk] payload bytes each.
/// A zero-length payload yields a single empty frame.
List<Uint8List> buildFrames(int msgType, Uint8List payload, int maxChunk) {
  if (maxChunk < 1) throw ArgumentError('maxChunk must be >= 1');
  if (payload.length > kMaxMessageLen) {
    throw ArgumentError('payload too large: ${payload.length}');
  }
  final frames = <Uint8List>[];
  final total = payload.length;
  var offset = 0;
  var seq = 0;
  do {
    final chunk = (total - offset) > maxChunk ? maxChunk : (total - offset);
    final frame = Uint8List(kFrameHeaderLen + chunk);
    final bd = ByteData.view(frame.buffer);
    bd.setUint8(0, kProtocolVersion);
    bd.setUint8(1, msgType);
    bd.setUint16(2, seq, Endian.big);
    bd.setUint16(4, total, Endian.big);
    bd.setUint16(6, chunk, Endian.big);
    frame.setRange(kFrameHeaderLen, kFrameHeaderLen + chunk, payload, offset);
    frames.add(frame);
    offset += chunk;
    seq++;
  } while (offset < total);
  return frames;
}

/// A completed reassembled message.
class ReasmMessage {
  ReasmMessage(this.msgType, this.payload);
  final int msgType;
  final Uint8List payload;
}

/// Reassembles frames for a single characteristic (protocol-spec §3).
class Reassembler {
  int _msgType = 0;
  int _totalLen = 0;
  int _nextSeq = 0;
  bool _active = false;
  BytesBuilder _buf = BytesBuilder();

  void reset() {
    _active = false;
    _nextSeq = 0;
    _totalLen = 0;
    _buf = BytesBuilder();
  }

  /// Feed one frame. Returns the completed message, or null if more are expected.
  /// Throws [FormatException] on a malformed frame (and resets).
  ReasmMessage? feed(Uint8List frame) {
    if (frame.length < kFrameHeaderLen) {
      reset();
      throw const FormatException('short frame');
    }
    final bd = ByteData.view(frame.buffer, frame.offsetInBytes, frame.length);
    final version = bd.getUint8(0);
    final msgType = bd.getUint8(1);
    final seq = bd.getUint16(2, Endian.big);
    final totalLen = bd.getUint16(4, Endian.big);
    final chunkLen = bd.getUint16(6, Endian.big);

    if (version != kProtocolVersion) {
      reset();
      throw FormatException('bad version 0x${version.toRadixString(16)}');
    }
    if (frame.length - kFrameHeaderLen < chunkLen) {
      reset();
      throw const FormatException('chunk_len exceeds frame');
    }

    if (seq == 0) {
      _active = true;
      _msgType = msgType;
      _totalLen = totalLen;
      _nextSeq = 0;
      _buf = BytesBuilder();
    } else if (!_active || seq != _nextSeq || msgType != _msgType) {
      reset();
      throw FormatException('unexpected seq $seq');
    }

    if (_buf.length + chunkLen > _totalLen) {
      reset();
      throw const FormatException('reassembly overflow');
    }
    _buf.add(frame.sublist(kFrameHeaderLen, kFrameHeaderLen + chunkLen));
    _nextSeq++;

    if (_buf.length == _totalLen) {
      final msg = ReasmMessage(_msgType, _buf.toBytes());
      reset();
      return msg;
    }
    return null;
  }
}
