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
| Secure Boot | Không xác định / không hỗ trợ qua lệnh |
| Test signing | **Chưa bật** |
| WDK/`vhf.h` | Có (WDK 10.0.26100, `vhfkm.lib`) |
| SDK/WDK | 10.0.26100.0 |
| Visual Studio | BuildTools 2022 |
| devcon | **Chưa có trong PATH** |\n| Driver VHF | **Compile OK** (`driver\build\fido2.sys`) |\n| Load driver | Chưa (test signing off) |
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
- [ ] Phase 2 CTAPHID
- [ ] Phase 3 getInfo
- [ ] Phase 4 makeCredential
- [ ] Phase 5 getAssertion
- [ ] Phase 6 WebAuthn test
