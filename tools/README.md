# tools/

## check-env.ps1

Kiểm tra môi trường FIDO2 dev, **không thay đổi gì**:

```powershell
powershell -ExecutionPolicy Bypass -File .\check-env.ps1
```

Nội dung kiểm tra:

- Secure Boot
- test signing
- WDK include `vhf.h`
- Visual Studio / MSBuild
- devcon
- Chrome/Edge

## Khi nào dùng

Chạy trước Phase 1 và sau khi bật/tắt Secure Boot để xác nhận trạng thái.
