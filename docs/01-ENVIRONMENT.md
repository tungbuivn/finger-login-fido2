# 01 — Môi trường dev

## Máy

- Windows 11 x64 (build hiện tại của máy dev)
- Quyền Administrator
- Nên dùng **máy dev riêng** hoặc VM snapshot; test signing ảnh hưởng toàn hệ thống

## 1. Tắt Secure Boot

VHF driver là kernel driver; test signing không có hiệu lực khi Secure Boot bật.

1. Backup/suspend **BitLocker** nếu đang bật:
   ```powershell
   manage-bde -status
   ```
2. Vào UEFI/BIOS → **Secure Boot: Disabled**
3. Lưu, boot lại Windows.
4. Kiểm tra:
   ```powershell
   Confirm-SecureBootUEFI
   ```
   Kết quả phải là lỗi/`False` (tùy máy), không phải `True`.

## 2. Bật test signing

PowerShell Admin:

```powershell
bcdedit /set testsigning on
shutdown /r /t 0
```

Sau reboot:

```powershell
bcdedit | findstr /i testsigning
```

Kỳ vọng:

```text
testsigning             Yes
```

Desktop có watermark **Test Mode**.

## 3. Cài WDK

Cần:

- Visual Studio 2022 (Desktop C++)
- **Windows Driver Kit (WDK)** tương ứng Windows 11
- Windows SDK
- **Spectre-mitigated libs** cho driver

Kiểm tra:

```powershell
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
  Select-Object Name
Test-Path "C:\Program Files (x86)\Windows Kits\10\Include\<ver>\km\vhf.h"
```

## 4. Công cụ test

- **devcon.exe** (WDK)
- HIDView / `hidview` hoặc PowerShell HID enumerate
- Chrome/Edge mới
- Trang test WebAuthn (localhost hoặc webauthn.io)

## 5. Revert sau khi dev

```powershell
bcdedit /set testsigning off
shutdown /r /t 0
```

Bật lại Secure Boot trong UEFI.

## Lưu ý

- Test signing **không** làm driver ổn định/production.
- Không dùng máy có dữ liệu quan trọng.
- Nếu công ty/quản trị chặn test signing, dừng dự án hoặc dùng VM.
