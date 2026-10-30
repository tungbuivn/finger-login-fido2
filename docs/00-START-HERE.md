# 00 — Bắt đầu ở đây

## Quyết định dự án

1. **WebAuthn website/passkey**, không phải Windows local login.
2. **Khóa phần mềm trên PC** trong MVP.
3. **VHF driver + test signing**, tắt Secure Boot khi dev.
4. PhoneUnlock/login hiện tại **không bị đụng tới**.

## Trạng thái

| Hạng mục | Trạng thái |
|---|---|
| Tài liệu thiết kế | Xong |
| Secure Boot | **Chưa tắt** |
| Test signing | **Chưa bật** |
| WDK/VS | Chưa xác nhận |
| Driver VHF | Chưa code |
| CTAP2 service | Chưa code |
| WebAuthn test | Chưa test |

## Khi quay lại làm tiếp

1. Đọc `01-ENVIRONMENT.md`.
2. Tắt Secure Boot trong UEFI (suspend BitLocker trước nếu có).
3. Bật test signing:
   ```powershell
   bcdedit /set testsigning on
   ```
4. Reboot; xác nhận watermark + `bcdedit`.
5. Cài WDK/SDK nếu chưa có.
6. Làm Phase 1 trong `03-PHASES.md`.

## Nguồn tham khảo

- Microsoft VHF: Virtual HID Framework
- CTAP2 spec (FIDO Alliance)
- WebAuthn Level 2/3
- FIDO HID usage page `0xF1D0`
