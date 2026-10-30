# 08 — Kernel debugging + test signing

## Trạng thái

- Secure Boot: tắt
- Test signing: bật
- Kernel debugging: bật (`bcdedit /debug on`)
- Chưa cấu hình transport debugger (serial/net/USB)

## Bật/tắt

PowerShell Admin:

```powershell
bcdedit /set testsigning on
bcdedit /debug on
bcdedit /enum "{current}"
```

Tắt:

```powershell
bcdedit /set debug off
bcdedit /set testsigning off
```

## Transport debugger

`/debug on` chỉ bật chế độ debug. Để WinDbg kết nối, cần cấu hình:

### Serial (COM)

```powershell
bcdedit /dbgsettings serial debugport:1 baudrate:115200
```

### Network

```powershell
bcdedit /dbgsettings net hostip:192.168.1.10 port:50000 key:1.2.3.4
```

### USB

```powershell
bcdedit /dbgsettings usb targetname:FIDO2DEV
```

Sau đó cắm cáp/khởi động WinDbg.

## Kiểm tra

```powershell
bcdedit | findstr /i "testsigning debug"
```

Sau reboot, desktop có watermark **Test Mode**.

## Lưu ý

- Không có debugger kết nối thì Windows vẫn boot bình thường, nhưng debug mode có thể làm chậm/khó ổn định.
- Không bật kernel debugging trên máy production.
- Driver test-signed chỉ load khi test signing + Secure Boot off.
- Revert trước khi dùng máy bình thường.
