# 07 — Resume checklist (sau khi tắt Secure Boot)

> Dùng mục này khi quay lại dự án sau một thời gian.

## 0. Trạng thái phải nhớ

- Dự án **độc lập**, không sửa PhoneUnlock.
- Mục tiêu: **WebAuthn website/passkey**
- Khóa: **phần mềm trên PC** (MVP)
- Driver: **VHF kernel driver**
- Signing: **test signing**, Secure Boot **tắt**
- Chưa có code; mới có docs.

## 1. Kiểm tra máy

```powershell
Confirm-SecureBootUEFI
bcdedit | findstr /i testsigning
```

Kỳ vọng:

- Secure Boot: không bật
- `testsigning Yes`

Nếu chưa:

```powershell
# UEFI: Secure Boot = Disabled trước
bcdedit /set testsigning on
shutdown /r /t 0
```

## 2. Kiểm tra WDK

```powershell
$kits = "C:\Program Files (x86)\Windows Kits\10\Include"
Get-ChildItem $kits | Select-Object Name
Get-ChildItem $kits -Recurse -Filter vhf.h | Select-Object -First 3 FullName
```

Nếu thiếu: cài Visual Studio 2022 + WDK + SDK + Spectre libs.

## 3. Đọc lại thiết kế

1. `02-ARCHITECTURE.md`
2. `03-PHASES.md`
3. `05-HID-DESCRIPTOR.md`
4. `04-CTAP2-NOTES.md`

## 4. Bắt đầu Phase 1

Tạo driver WDF tối thiểu:

- `DriverEntry`
- `EvtDeviceAdd`
- `VhfCreate`
- Report descriptor FIDO
- Gửi input report rỗng

Build:

```powershell
msbuild driver\fido2.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Cài test:

```powershell
devcon install driver\fido2.inf "Root\Fido2Virtual"
```

## 5. Tiêu chí Phase 1

- [ ] Driver load
- [ ] Device Manager thấy HID
- [ ] HIDView thấy Usage Page 0xF1D0
- [ ] `hidfido` bound

Nếu không, đọc `05-HID-DESCRIPTOR.md`.

## 6. Sau Phase 1

Làm Phase 2 (CTAPHID), rồi Phase 3 (getInfo). Chưa cần WebAuthn.

## 7. Khi kết thúc dev

```powershell
bcdedit /set testsigning off
shutdown /r /t 0
```

Bật lại Secure Boot.

## 8. Câu hỏi cần quyết khi resume

- Có cần **PIN/UV** ngay không? (một số RP bắt buộc)
- Có cần **resident/discoverable credential** không?
- Có chuyển khóa sang **TPM** không?
- Có dùng **phone signer** không?

Ghi câu trả lời vào `README.md` hoặc `03-PHASES.md`.
