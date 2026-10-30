# 06 — Test plan

## Phase 1 — Enumerate

- [ ] Driver load không lỗi
- [ ] Device Manager có HID device
- [ ] Hardware ID đúng VID/PID
- [ ] HIDView thấy Usage Page 0xF1D0, Usage 0x01
- [ ] `hidfido` bound

Lệnh:

```powershell
Get-Service hidfido
Get-PnpDevice -Class HIDClass | Format-Table -AutoSize
```

## Phase 2 — CTAPHID

- [ ] Gửi `CTAPHID_INIT` qua hidtest
- [ ] Nhận nonce/CID đúng
- [ ] `CTAPHID_PING` echo đúng
- [ ] Continuation packet hoạt động với payload > 57 byte

## Phase 3 — getInfo

- [ ] Chrome/Edge nhận security key
- [ ] `authenticatorGetInfo` trả CBOR hợp lệ
- [ ] Không lỗi `CTAP1_ERR_INVALID_COMMAND`

## Phase 4/5 — WebAuthn

Test local RP (khuyến nghị):

```bash
# ví dụ SimpleWebAuthn / webauthn-rp local
https://localhost:8443
```

Hoặc:

```text
https://webauthn.io
https://www.passkeys.io
```

Các bước:

1. Đăng ký passkey
2. Đăng xuất
3. Đăng nhập bằng passkey
4. Restart service/driver, thử lại
5. Reboot, thử lại

## Log cần thu

- Service log: CTAPHID command, CTAP2 command, status
- Chrome `chrome://device-log` hoặc WebAuthn DevTools
- Windows Event Viewer nếu driver lỗi

## Tiêu chí pass MVP

- [ ] Đăng ký passkey thành công trên 1 RP
- [ ] Đăng nhập lại thành công
- [ ] Sau reboot vẫn hoạt động
- [ ] Không crash driver/service
- [ ] Không cần USB token thật

## Tiêu chí chưa đạt

- RP yêu cầu attestation chặt
- RP yêu cầu UV/PIN
- Windows Hello/passwordless MSA
- Local account login
