# blifi Protocol Specification

**Status:** v1 - reference document. The `blifi` ESP-IDF component and the
`blifi` Flutter package MUST both implement this exactly; it is the contract
that lets them interoperate byte-for-byte.

**Related:** cryptographic handshake and key schedule live in
[`security.md`](security.md); rationale in [`adr/0001-security-scheme.md`](adr/0001-security-scheme.md).

Protocol version (`version` byte in every frame): **`0x01`**.

---

## 1. Layers

```
JSON payload  (application message, e.g. Wi-Fi credentials)
   │  encrypt (AES-256-GCM) - encrypted channels only
Encrypted record  = counter(4) ‖ ciphertext ‖ tag(16)
   │  chunk to fit MTU
Frames  = [header(8) ‖ chunk] written to / notified on a GATT characteristic
```

All multi-byte integers in headers and records are **big-endian (network order)**.

---

## 2. GATT service

One custom 128-bit service. Characteristic UUIDs share the service's base, with
byte 12-13 (`0x000N`) selecting the characteristic.

| Role | UUID | Properties | Enc? |
|------|------|-----------|------|
| **Service** | `6b1a0001-5f3e-4b7a-9c2d-1e8f7a4c9b20` | - | - |
| Device-Info | `6b1a0002-5f3e-4b7a-9c2d-1e8f7a4c9b20` | Read | plaintext |
| Handshake | `6b1a0003-5f3e-4b7a-9c2d-1e8f7a4c9b20` | Write, Notify | plaintext¹ |
| Wi-Fi-Scan | `6b1a0004-5f3e-4b7a-9c2d-1e8f7a4c9b20` | Write, Notify | encrypted |
| Credentials | `6b1a0005-5f3e-4b7a-9c2d-1e8f7a4c9b20` | Write | encrypted |
| Status | `6b1a0006-5f3e-4b7a-9c2d-1e8f7a4c9b20` | Notify | encrypted |

¹ Handshake carries only public keys, HMAC confirmation tags, and a failure code
- none are secret, so the characteristic is plaintext. See `security.md`.

The app subscribes (CCCD) to Handshake, Wi-Fi-Scan, and Status notifications
before starting the handshake.

---

## 3. Framing

Every write and every notification is one **frame**:

```
 0        1         2      4          6          8
 +--------+---------+------+----------+----------+-----------------+
 |version | msg_type|  seq | total_len| chunk_len|   chunk bytes   |
 |  u8    |   u8    | u16  |   u16    |   u16    |   (chunk_len)   |
 +--------+---------+------+----------+----------+-----------------+
             8-byte header
```

- `version` - `0x01`.
- `msg_type` - see §4.
- `seq` - chunk index within this message, from `0`.
- `total_len` - total length of the **reassembled payload** (the encrypted
  record for encrypted types, or the raw message bytes for plaintext types).
  Max `65535`.
- `chunk_len` - bytes of payload in this frame.

**Reassembly:** the receiver concatenates chunk bytes in `seq` order until the
accumulated length equals `total_len`, then processes the payload. One message
is in flight per characteristic at a time; a frame with `seq == 0` starts a new
message and resets any partial buffer for that characteristic.

**MTU / chunking:** negotiate the largest ATT MTU the platform allows on
connect. Usable bytes per frame = `MTU − 3` (ATT header); therefore
`chunk_len ≤ MTU − 3 − 8`. If MTU negotiation fails or is capped (e.g. iOS),
chunking still works at the smaller size. A single-frame message uses
`seq=0, total_len=chunk_len`.

---

## 4. Message types

| `msg_type` | Name | Channel | Direction | Enc? | Payload |
|-----------|------|---------|-----------|------|---------|
| `0x01` | `DEVICE_INFO` | Device-Info | dev→app | plaintext | JSON (§6.1) |
| `0x02` | `HS_PUBKEY` | Handshake | both | plaintext | 32-byte X25519 public key |
| `0x03` | `HS_CONFIRM` | Handshake | both | plaintext | 16-byte HMAC tag (§ security.md) |
| `0x04` | `HS_FAIL` | Handshake | dev→app | plaintext | 1 byte: status code (`AUTH_FAILED`) |
| `0x10` | `SCAN_REQUEST` | Wi-Fi-Scan | app→dev | encrypted | JSON (§6.2) |
| `0x11` | `SCAN_RESPONSE` | Wi-Fi-Scan | dev→app | encrypted | JSON (§6.3) |
| `0x20` | `CREDENTIALS` | Credentials | app→dev | encrypted | JSON (§6.4) |
| `0x30` | `STATUS` | Status | dev→app | encrypted | JSON (§6.5) |

---

## 5. Encrypted record

Encrypted messages (`0x10`-`0x30`) carry an AES-256-GCM record as the framing
payload:

```
record = counter(4, BE) ‖ ciphertext(N) ‖ tag(16)
```

- Key: the per-direction session key (`K_app2dev` or `K_dev2app`) from the
  handshake - see `security.md`.
- Nonce (96-bit) = `00 00 00 00 00 00 00 00 ‖ counter(4, BE)`.
- `counter` - per-direction, starts at `0`, +1 per message sent on that key.
  Carried in the record so the receiver builds the nonce and enforces replay
  protection (**reject any counter ≤ the last accepted** on that key).
- AAD = `version(1) ‖ msg_type(1)`.
- `tag` - 16-byte GCM authentication tag. A failed tag check ⇒ drop + treat as
  `INVALID_MESSAGE`.
- Plaintext = the UTF-8 JSON of §6.

---

## 6. Payload schemas (decrypted JSON)

### 6.1 `DEVICE_INFO` (dev→app, plaintext)
```json
{ "proto": 1, "fw": "1.0.0", "name": "blifi-A1B2",
  "state": "unprovisioned", "pop_required": true }
```
`state` ∈ `unprovisioned | provisioned | connecting | connected`.

### 6.2 `SCAN_REQUEST` (app→dev)
```json
{ "refresh": true }
```

### 6.3 `SCAN_RESPONSE` (dev→app)
```json
{ "networks": [
  { "ssid": "HomeWiFi", "rssi": -47, "auth": 3, "channel": 6, "hidden": false }
] }
```
`auth` = ESP-IDF `wifi_auth_mode_t` numeric: `0`=open, `1`=WEP, `2`=WPA-PSK,
`3`=WPA2-PSK, `4`=WPA/WPA2-PSK, `5`=WPA2-Enterprise, `6`=WPA3-PSK,
`7`=WPA2/WPA3-PSK. Results may span multiple frames (chunked).

### 6.4 `CREDENTIALS` (app→dev)
```json
{ "ssid": "HomeWiFi", "password": "hunter2", "bssid": "aa:bb:cc:dd:ee:ff", "channel": 6 }
```
`bssid` and `channel` are optional hints to speed up connection.

### 6.5 `STATUS` (dev→app)
```json
{ "code": 18, "detail": "got ip", "ip": "192.168.1.42" }
```
`code` = decimal of the §7 enum. `ip` present only on `WIFI_CONNECTED`.
`detail` is a human-readable string (never parsed for logic).

---

## 7. Status / error codes

Single `u8` space, mirrored exactly in firmware C (`blifi_status_t`) and Dart
(`ProvisioningStatus`). `0x00`-`0x1F` = states, `0x20`+ = errors.

| Code | Name | Meaning |
|------|------|---------|
| `0x00` | `IDLE` | No provisioning in progress |
| `0x01` | `HANDSHAKE_IN_PROGRESS` | Keys being negotiated |
| `0x02` | `HANDSHAKE_OK` | Session established (confirmation passed) |
| `0x10` | `CREDENTIALS_RECEIVED` | Device decrypted credentials |
| `0x11` | `WIFI_CONNECTING` | Attempting to join the AP |
| `0x12` | `WIFI_CONNECTED` | Online; `ip` provided |
| `0x20` | `AUTH_FAILED` | Wrong PoP / confirmation mismatch |
| `0x21` | `WIFI_NOT_FOUND` | Target SSID not found |
| `0x22` | `WIFI_AUTH_ERROR` | Wrong Wi-Fi password |
| `0x23` | `WIFI_TIMEOUT` | Association/DHCP timed out |
| `0x24` | `WIFI_DISCONNECTED` | Lost connection after joining |
| `0x25` | `PROV_TIMEOUT` | Provisioning window elapsed |
| `0x26` | `INVALID_MESSAGE` | Malformed/failed-auth frame |
| `0x27` | `INTERNAL_ERROR` | Unexpected device fault |

**Dart exception mapping:** `AUTH_FAILED` → `AuthenticationException`;
`WIFI_NOT_FOUND` / `WIFI_AUTH_ERROR` / `WIFI_DISCONNECTED` →
`WifiConnectionException(code)`; `WIFI_TIMEOUT` / `PROV_TIMEOUT` →
`ProvisioningTimeoutException`; `INVALID_MESSAGE` / `INTERNAL_ERROR` →
`BleConnectionException` / protocol error.

---

## 8. End-to-end sequence

```
app                                                   device
 │  read Device-Info  ────────────────────────────────▶ │
 │ ◀───────────── {proto,fw,name,state,pop_required}     │
 │  subscribe Handshake / Wi-Fi-Scan / Status            │
 │  HS_PUBKEY(app_pub)  ───────────────────────────────▶ │
 │ ◀───────────────────────────────  HS_PUBKEY(dev_pub)  │
 │        (both derive K_app2dev, K_dev2app, K_confirm)   │
 │  HS_CONFIRM(confirm_app)  ─────────────────────────▶  │ verify
 │ ◀──────────────────  HS_CONFIRM(confirm_dev)  [or HS_FAIL(AUTH_FAILED)]
 │  == encrypted from here ==                            │
 │  SCAN_REQUEST  ────────────────────────────────────▶  │
 │ ◀──────────────────────────  SCAN_RESPONSE(networks)  │
 │  CREDENTIALS(ssid,password)  ─────────────────────▶   │ store→connect
 │ ◀──── STATUS(CREDENTIALS_RECEIVED) … WIFI_CONNECTING … │
 │ ◀──── STATUS(WIFI_CONNECTED, ip)  [or error code]      │
```

---

## 9. Versioning & compatibility

- A receiver MUST reject frames whose `version` it does not support
  (`INVALID_MESSAGE`).
- New `msg_type` values and new JSON fields are additive; implementations MUST
  ignore unknown JSON fields and unknown `msg_type`s (log + drop) rather than
  fail hard, to allow forward-compatible extensions (e.g. future OTA channel).
- Any change to framing, the encrypted-record layout, the key schedule, or code
  reassignments is a **breaking** change and MUST bump `version`.
