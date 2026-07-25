# ADR 0001 - Credential-transfer security scheme

- **Status:** Accepted (2026-07-23)
- **Related:** [`../security.md`](../security.md), [`../protocol-spec.md`](../protocol-spec.md)

## Context

Wi-Fi credentials must travel from the phone to the ESP32 over BLE without ever
being exposed in the clear, and without an active man-in-the-middle being able
to capture them or impersonate the device. BLE advertising/GATT traffic is
trivially sniffable, so transport must be encrypted and authenticated, and the
scheme must be implementable identically in firmware (mbedTLS/C) and in Dart.

## Decision

Establish a session with **ephemeral X25519 ECDH**, derive keys with
**HKDF-SHA256** while mixing in a **Proof-of-Possession (PoP)** string, confirm
the handshake with a **truncated HMAC-SHA256** exchange, and encrypt all
subsequent messages with **AES-256-GCM** (96-bit counter nonce per direction,
128-bit tag). Full construction in `security.md`.

Product posture (this project's defaults):
- **PoP required by default**; a no-PoP build flag exists for development only.
- PoP is an **8-char Crockford base32** value (~40 bits), generated on first
  boot, stored in NVS, logged once over UART, and swappable for an OEM/QR source.
- **Explicit key confirmation** before any credential is sent, so a wrong PoP
  fails fast as `AUTH_FAILED`.

## Consequences

- Strong, standard guarantees: confidentiality, integrity, mutual auth, forward
  secrecy, replay protection.
- Both codebases depend only on widely available primitives (mbedTLS on device;
  a maintained X25519/HKDF/AES-GCM Dart package in the app).
- The firmware carries a hard obligation to **rate-limit** confirmation attempts,
  since ~40-bit PoP + no built-in AEAD throttle would otherwise allow online
  guessing.
- Interop is fragile to spec drift - the exact byte layout, HKDF labels, and
  nonce discipline are pinned in `security.md`/`protocol-spec.md` and any change
  bumps the protocol version.

## Alternatives considered

- **ESP-IDF `protocomm` security2 (SRP6a PAKE):** stronger password-authenticated
  key exchange, but heavier to mirror byte-for-byte in Dart and more than a
  ~40-bit PoP needs here. Revisit if PoP entropy is lowered.
- **Static pre-shared key:** no forward secrecy and an awkward key-distribution
  problem; rejected.
- **TLS/DTLS over BLE (e.g. via a PSK ciphersuite):** heavyweight for a
  once-per-device provisioning exchange and awkward over GATT; rejected.
