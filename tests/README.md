# tests/

Chưa có code. Kế hoạch:

```text
tests/
  rp/                 # WebAuthn relying party local
  scripts/
  logs/
```

## Mục tiêu

- Đăng ký passkey bằng virtual authenticator
- Đăng nhập lại
- Test sau reboot

## Local RP

Có thể dùng:

- `@simplewebauthn/server` + Express
- `webauthn-rp` Python
- Bất kỳ RP nào chạy `https://localhost`

Yêu cầu:

- HTTPS (Chrome yêu cầu secure context; localhost được phép)
- RP ID khớp domain
- Lưu credential public key

## Test nhanh

```text
https://webauthn.io
https://www.passkeys.io
```

Xem `docs/06-TEST-PLAN.md`.
