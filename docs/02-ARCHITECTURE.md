# 02 — Kiến trúc

## Thành phần

```text
Chrome/Edge / WebAuthn RP
        |
        | Windows WebAuthn API + hidfido.sys
        v
+--------------------------------------+
| Virtual HID device (VHF)             |
| Usage Page 0xF1D0, Usage 0x01        |
+------------------+-------------------+
                   | HID reports 64 byte
                   v
+--------------------------------------+
| fido2svc.exe (user mode)             |
| - CTAPHID framing                    |
| - CTAP2 CBOR                         |
| - makeCredential / getAssertion      |
| - credential store (DPAPI)           |
+--------------------------------------+
```

## Driver

- WDF/KMDF driver
- Dùng **VHF** (`VhfCreate`) tạo HID device ảo
- Không cần driver USB; Windows inbox `hidfido` bind vào HID FIDO
- Driver **không** giữ private key
- Giao tiếp với service qua:
  - Device interface + IOCTL, hoặc
  - Shared queue / pipe do driver tạo

### HID report

- Report size: **64 byte**
- Descriptor tối thiểu: Input + Output report, Usage Page FIDO
- Chi tiết: `05-HID-DESCRIPTOR.md`

## User-mode service

- Chạy như Windows service (hoặc console khi dev)
- Mở device interface do driver tạo
- Đọc/ghi HID reports
- Parse **CTAPHID** → **CTAP2 CBOR**
- Trả response

### Lưu khóa

MVP:

- Sinh ES256 keypair phần mềm
- Lưu private key bằng **DPAPI CurrentUser/Machine** hoặc **CNG persisted key**
- Public key trong credential record

Phase sau:

- TPM-backed key
- Phone signer qua Bluetooth
- PIN/UV

## Luồng makeCredential

1. RP gửi `navigator.credentials.create`
2. Windows `hidfido` gửi CTAPHID CBOR `authenticatorMakeCredential`
3. Service tạo keypair, credential ID, trả attestation `none`
4. Windows trả public key cho RP

## Luồng getAssertion

1. RP gửi challenge
2. Windows gửi `authenticatorGetAssertion`
3. Service tìm credential theo RP ID + allowList
4. Ký `authenticatorData || SHA256(clientDataJSON)`
5. Trả assertion

## Bảo mật MVP

- Không có user verification thật; `up=1` do phần mềm
- Attestation `none`
- Không phù hợp production
- Ghi rõ trong docs/test
