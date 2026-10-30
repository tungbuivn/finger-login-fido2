# STATUS

Cập nhật: 2026-10-30

## Quyết định

- WebAuthn website/passkey
- Khóa phần mềm trên PC (MVP)
- VHF kernel driver
- Test signing + tắt Secure Boot khi dev
- Không đụng PhoneUnlock/login

## Môi trường hiện tại (đã kiểm tra)

| Mục | Kết quả |
|---|---|
| Secure Boot | **UEFISecureBootEnabled=0** (đã tắt) |
| Test signing | **Đã bật** (`testsigning Yes`), cần reboot |
| WDK/`vhf.h` | Có (WDK 10.0.26100, `vhfkm.lib`) |
| SDK/WDK | 10.0.26100.0 |
| Visual Studio | BuildTools 2022 |
| devcon | Có: `C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe` |\n| Driver VHF | **Chạy OK**, HID FIDO `UP:F1D0` |\n| Load driver | **OK** — `HID-compliant fido`, service `fido2fido` |
| Browser | Edge có; Chrome chưa thấy ở đường dẫn chuẩn |

## Việc tiếp theo

1. Xác nhận/tắt Secure Boot trong UEFI.
2. `bcdedit /set testsigning on` + reboot.
3. Thêm `devcon.exe` (WDK Tools) vào PATH.
4. Làm Phase 1.

## Phase

- [x] Tài liệu
- [ ] Phase 0 môi trường
- [x] Phase 1 code compile\n- [ ] Phase 1 load/enumerate
- [x] Phase 2 CTAPHID INIT/PING
- [x] Phase 3 getInfo\n- [x] makeCredential tạo response 177 (status 00)\n- [ ] WebAuthn browser test
- [ ] Phase 4 makeCredential
- [ ] Phase 5 getAssertion
- [ ] Phase 6 WebAuthn test
