# FIDO2 Virtual Authenticator (Windows)

Dự án **độc lập**, không thuộc build/runtime của PhoneUnlock.

## Mục tiêu

- Windows thấy một **FIDO2 security key ảo** qua **Virtual HID Framework (VHF)**
- Dùng được cho **WebAuthn / passkey trên website** (Chrome/Edge)
- Private key nằm trên **PC** trong MVP, bảo vệ bằng DPAPI/TPM sau
- Không cần USB phần cứng, không cần driver USB riêng
- Chấp nhận **test signing + tắt Secure Boot** trong giai đoạn dev

## Không thuộc phạm vi

- Không thay đổi PhoneUnlock, Credential Provider, Windows login
- Không dùng FIDO2 để unlock **local account Windows** (Windows không hỗ trợ)
- Không làm token phần cứng
- Không hướng tới production signing trong giai đoạn đầu

## Quyết định đã chốt

| Hạng mục | Quyết định |
|---|---|
| Mục tiêu | WebAuthn website / passkey |
| Khóa | Phần mềm trên PC (MVP) |
| Driver | VHF kernel driver |
| Signing | Test signing, tắt Secure Boot khi dev |
| Phone | Chưa dùng trong MVP; có thể làm signer ở phase sau |

## Cấu trúc

```text
fido2/
  README.md
  docs/
    00-START-HERE.md
    01-ENVIRONMENT.md
    02-ARCHITECTURE.md
    03-PHASES.md
    04-CTAP2-NOTES.md
    05-HID-DESCRIPTOR.md
    06-TEST-PLAN.md
    07-RESUME-CHECKLIST.md
  driver/    # WDF/VHF driver (chưa code)
  service/   # user-mode CTAP2 service (chưa code)
  tools/     # devcon/hid test scripts
  tests/     # WebAuthn RP test
```

## Trạng thái

**Chưa bắt đầu code.** Mới có tài liệu thiết kế và checklist.

Bước tiếp theo: đọc `docs/07-RESUME-CHECKLIST.md`, tắt Secure Boot, bật test signing, cài WDK, rồi làm Phase 1.

## Cảnh báo

- Test signing làm giảm bảo mật hệ thống; chỉ dùng máy dev.
- Software token **không** bảo vệ khóa tốt như security key phần cứng.
- WebAuthn có thể yêu cầu user verification/PIN; MVP sẽ xử lý dần.
