# blifi Security Design

**Status:** v1 — reference document, implemented identically by the `blifi`
ESP-IDF component and the `blifi` Flutter package. Wire formats are in
[`protocol-spec.md`](protocol-spec.md); rationale in
[`adr/0001-security-scheme.md`](adr/0001-security-scheme.md).

The goal: transfer Wi-Fi credentials from phone to device over BLE such that
they are never exposed in the clear and cannot be captured by an eavesdropper or
an active man-in-the-middle who does not know the device's Proof-of-Possession.

---

## 1. Threat model

**In scope (defended):**
- **Passive eavesdropper** sniffing BLE — must not learn SSID/password.
- **Active MITM** relaying/altering BLE traffic — must not obtain the session
  key or inject/modify credentials undetected.
- **Replay** of previously captured frames.

**Out of scope (v1):**
- A compromised phone or malicious provisioning app (it holds the plaintext).
- Physical extraction from the device (mitigated by flash/NVS encryption — §7).
- Denial of service by radio jamming.

---

## 2. Primitives

| Purpose | Primitive |
|---------|-----------|
| Key agreement | **X25519** (RFC 7748), ephemeral per session |
| Key derivation | **HKDF-SHA256** (RFC 5869) |
| Key confirmation | **HMAC-SHA256** (truncated to 128 bits) |
| Authenticated encryption | **AES-256-GCM**, 96-bit nonce, 128-bit tag |
| Randomness | firmware: `esp_random()` (HW RNG) / mbedTLS CTR_DRBG; app: platform CSPRNG |

Firmware uses mbedTLS (bundled with ESP-IDF): `mbedtls_ecdh` with
`MBEDTLS_ECP_DP_CURVE25519`, `mbedtls_hkdf`, `mbedtls_md` (HMAC), `mbedtls_gcm`.

---

## 3. Handshake

1. App reads **Device-Info** (plaintext) and learns `pop_required`.
2. Both sides generate an **ephemeral X25519 keypair**. Public keys are exchanged
   as raw 32-byte values via `HS_PUBKEY` (app writes first; device notifies its
   own). Ephemeral keys give **forward secrecy** — a later key compromise cannot
   decrypt a past session.
3. Both compute the ECDH shared secret `ecdh` (32 bytes).
4. Both derive the session keys (§4), mixing in the **PoP**.
5. Both run the **confirmation** exchange (§5). On success the session is live;
   all subsequent channel messages are AES-256-GCM encrypted.

Because the public-key exchange, confirmation tags, and any failure code are not
secret, the Handshake characteristic is plaintext.

---

## 4. Key schedule

```
ecdh   = X25519(own_priv, peer_pub)                      # 32 bytes, equal on both sides
salt   = app_pub ‖ device_pub                            # 64 bytes, fixed order
IKM    = ecdh ‖ utf8(PoP)                                # PoP omitted only in no-PoP build
PRK    = HKDF-Extract(salt, IKM)          = HMAC-SHA256(salt, IKM)

K_app2dev = HKDF-Expand(PRK, "blifi/v1 key app->dev", 32)
K_dev2app = HKDF-Expand(PRK, "blifi/v1 key dev->app", 32)
K_confirm = HKDF-Expand(PRK, "blifi/v1 confirm",      32)
```
`HKDF-Expand(PRK, info, 32) = HMAC-SHA256(PRK, info ‖ 0x01)` (single block).

Mixing PoP into the **secret** IKM is the crux: an attacker who observes the
public keys but does not know the PoP derives a *different* `PRK`, so their
session keys and confirmation tag will not match — exactly the property ESP-IDF's
own `protocomm` security schemes rely on. Using the two public keys as the HKDF
salt binds the derived keys to this specific handshake transcript.

Separate keys per direction mean the two GCM nonce-counter spaces never collide.

---

## 5. Key confirmation

```
confirm_app = HMAC-SHA256(K_confirm, "blifi/v1 confirm app" ‖ app_pub ‖ device_pub)[:16]
confirm_dev = HMAC-SHA256(K_confirm, "blifi/v1 confirm dev" ‖ app_pub ‖ device_pub)[:16]
```

- App sends `HS_CONFIRM(confirm_app)`. Device recomputes and compares in
  **constant time**.
- Mismatch ⇒ device replies `HS_FAIL(AUTH_FAILED)` (plaintext) and disconnects.
  It counts the attempt for lockout (§6).
- Match ⇒ device replies `HS_CONFIRM(confirm_dev)`; the app verifies it (also
  constant time). This proves to the app that the device knew the PoP too
  (mutual authentication), defeating a rogue device that lured the user.

A wrong PoP is thus detected **before** any credential is sent, surfacing as a
clean `AUTH_FAILED`.

---

## 6. AEAD usage & replay

- Each direction encrypts with its own key and a **strictly increasing 32-bit
  counter** starting at 0; nonce = `0^64 ‖ counter`. Never reuse a counter under
  a key (GCM nonce reuse is catastrophic — separate keys + monotonic counters
  guarantee this within a session, and ephemeral keys make every session fresh).
- The counter is sent in each record; the receiver **rejects any counter ≤ the
  last accepted** on that key, giving replay protection. Out-of-range or
  tag-invalid records are dropped as `INVALID_MESSAGE`.
- AAD binds `version` and `msg_type`, so a record cannot be replayed as a
  different message type.

---

## 7. Proof-of-Possession (PoP) trust model

- **Required by default.** Provisioning cannot complete without the correct PoP.
  A compile-time **no-PoP build flag** exists for development only; with it the
  handshake is merely *passively* secure (an active MITM can succeed), so it MUST
  NOT ship in production. `pop_required` in Device-Info tells the app which mode
  the device is in.
- **Format:** 8-character **Crockford base32** (~40 bits of entropy, e.g.
  `K7M2QP9X`), generated with the hardware RNG on first boot, stored in NVS, and
  **logged once over UART** for whoever flashes the device. The value is stable
  across reboots until credentials are reset.
- **Distribution:** the default "read it off serial" model suits hobby/maker use.
  The PoP source is **swappable** — an OEM can inject a per-device PoP printed on
  a QR label/sticker without any protocol change. This trust assumption is stated
  here explicitly so it is never an undocumented default.
- **Online brute-force resistance:** ~40 bits is ample against remote guessing,
  but because GCM gives no inherent rate-limit, the firmware **MUST** limit
  confirmation attempts. Recommended default: **lock provisioning for 30 s after
  5 consecutive `AUTH_FAILED`**, backing off further on repeats (tunable; exact
  thresholds finalized at implementation time). Each BLE reconnect does not reset
  the counter.

---

## 8. Security properties

- **Confidentiality & integrity** of all credentials via AES-256-GCM.
- **Mutual authentication** via the PoP-bound confirmation exchange.
- **Forward secrecy** from ephemeral X25519 keypairs.
- **Replay protection** from monotonic per-direction counters.
- **Downgrade note:** the no-PoP dev flag is the only weakening; it is opt-in,
  advertised via `pop_required`, and disallowed in production builds.

---

## 9. Hardening for production

Recommended (not required for the open-source default build), to be documented
per-deployment:
- **NVS encryption** for stored credentials + PoP, and **flash encryption** +
  **secure boot** to protect against physical readout.
- Suppress or gate the one-time PoP serial log in production firmware (or move to
  a factory-only path), so the PoP is not printed on shipped devices.
- Consider a shorter provisioning timeout and disabling re-provisioning triggers
  that a bystander could reach.
