# 03 — Phases

Đánh dấu `[ ]` khi xong. Không chuyển phase nếu acceptance chưa đạt.

## Phase 0 — Môi trường

- [ ] Secure Boot tắt
- [ ] `bcdedit` testsigning = Yes
- [ ] Visual Studio + WDK + SDK cài
- [ ] `vhf.h` tồn tại
- [ ] devcon/HID tool chạy

**Acceptance:** build được driver sample VHF trống, load được.

---

## Phase 1 — Virtual FIDO HID device

- [x] WDM VHF function driver chạy
- [x] Report descriptor FIDO, HID `UP:F1D0`
- [ ] Input + Output report 64 byte
- [x] Device Manager: HID-compliant fido
- [x] Windows HID stack nhận FIDO (không cần hidfido.sys)

**Acceptance:** Windows nhận thiết bị HID FIDO; chưa cần CTAP2.

**Rủi ro:** descriptor sai → `hidfido` không bind; kiểm tra top-level collection.

---

## Phase 2 — CTAPHID transport

- [ ] Service mở device interface
- [ ] Đọc/ghi report 64 byte
- [ ] Parse INIT / PING
- [ ] Trả `CTAPHID_INIT` đúng
- [ ] Ghép/gửi continuation packet

**Acceptance:** `hidtest`/Chrome thấy authenticator sống, ping được.

---

## Phase 3 — CTAP2 getInfo

- [ ] CBOR encode/decode
- [ ] `authenticatorGetInfo` (0x04)
- [ ] Trả versions `FIDO_2_0`, `FIDO_2_1`
- [ ] AAGUID giả/dev
- [ ] options: `rk`, `up`, `uv` (uv=false ban đầu)

**Acceptance:** Chrome/Edge hiển thị security key; getInfo thành công.

---

## Phase 4 — MakeCredential

- [ ] Parse CBOR request
- [ ] RP ID / user / challenge
- [ ] ES256 keypair
- [ ] Credential ID
- [ ] Auth data + `none` attestation
- [ ] Lưu credential DPAPI

**Acceptance:** WebAuthn RP đăng ký passkey thành công.

---

## Phase 5 — GetAssertion

- [ ] Tìm credential theo RP ID/allowList
- [ ] Tăng signCount
- [ ] Ký `authData || SHA256(clientDataJSON)`
- [ ] Trả assertion + user handle

**Acceptance:** Đăng nhập lại bằng passkey thành công.

---

## Phase 6 — WebAuthn test

- [ ] Test local RP
- [ ] Test webauthn.io
- [ ] Chrome passkey settings
- [ ] Log request/response

**Acceptance:** register + login ổn định sau reboot.

---

## Phase 7 — PIN/UV (nếu cần)

- [ ] `clientPIN` protocol
- [ ] PIN hash/retry
- [ ] UV=1
- [ ] RP yêu cầu UV hoạt động

---

## Phase 8 — Hardening (optional)

- [ ] TPM/CNG key
- [ ] Attestation
- [ ] Resident keys / discoverable credentials
- [ ] Phone signer
- [ ] Production signing

---

## Trạng thái hiện tại

Dừng ở **Phase 0**. Chưa có code.
