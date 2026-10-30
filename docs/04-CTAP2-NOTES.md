# 04 — CTAP2 notes

## CTAPHID

Mỗi HID report **64 byte**.

### Initialization packet

```text
offset  size  field
0       4     CID (big-endian)
4       1     command
5       2     payload length (big-endian)
7       57    payload
```

### Continuation packet

```text
0       4     CID
4       1     0x00 | sequence (high bit 0)
5       59    payload
```

### Commands

| Command | Value |
|---|---|
| CTAPHID_MSG | 0x80 |
| CTAPHID_CBOR | 0x90 |
| CTAPHID_INIT | 0x86 |
| CTAPHID_PING | 0x81 |
| CTAPHID_CANCEL | 0x91 |
| CTAPHID_KEEPALIVE | 0xBB |
| CTAPHID_ERROR | 0xBF |

FIDO2 dùng **CTAPHID_CBOR (0x90)**.

## CTAP2 commands

| Command | Value |
|---|---|
| makeCredential | 0x01 |
| getAssertion | 0x02 |
| getInfo | 0x04 |
| clientPIN | 0x06 |
| reset | 0x07 |
| getNextAssertion | 0x08 |
| credentialManagement | 0x0A |

## CBOR tối thiểu

- Map, text string, byte string, int, bool
- COSE key ES256:
  - `1 (kty) = 2 (EC2)`
  - `3 (alg) = -7 (ES256)`
  - `-1 (crv) = 1 (P-256)`
  - `-2 x`, `-3 y` (32 byte)

## Authenticator data

```text
rpIdHash        32
flags           1   (UP=0x01, UV=0x04, AT=0x40, ED=0x80)
signCount       4
attestedCredData (nếu AT)
extensions      (nếu ED)
```

## Attestation MVP

- `fmt = "none"`
- `attStmt = {}`
- Không cần attestation certificate

## User presence / verification

- MVP: `up=1` do service tự đặt, không có nút bấm thật
- `uv=0`
- Nếu RP yêu cầu `userVerification=required`, cần phase PIN/UV
- WebAuthn passkey trên Chrome có thể yêu cầu UV tùy policy

## Credential store

```json
{
  "rpId": "example.com",
  "credentialId": "base64url",
  "userHandle": "base64url",
  "privateKeyDpapi": "base64",
  "signCount": 1,
  "createdAt": "..."
}
```

MVP lưu file DPAPI; phase sau TPM.

## Lỗi CTAP2

- `0x01 CTAP1_ERR_INVALID_COMMAND`
- `0x02 INVALID_PARAMETER`
- `0x03 INVALID_LENGTH`
- `0x14 INTEGRITY_FAILURE`
- `0x27 INVALID_CREDENTIAL`
- `0x2B NO_CREDENTIALS`
- `0x31 PIN_INVALID`
- `0x34 PIN_AUTH_INVALID`
- `0x35 PIN_NOT_SET`

## Tài liệu

- FIDO CTAP 2.1
- WebAuthn Level 2
- FIDO HID 1.0
