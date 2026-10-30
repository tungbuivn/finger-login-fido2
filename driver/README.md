# driver/

Chưa có code. Kế hoạch file:

```text
driver/
  fido2.inf
  fido2.vcxproj
  driver.c / driver.h
  device.c
  queue.c
  hid.c
  trace.h
```

## Trách nhiệm

- Tạo VHF device
- Report descriptor FIDO
- Gửi/nhận HID report 64 byte
- IOCTL/device interface cho service
- Không giữ private key

## Bắt đầu

1. Copy mẫu **VHF sample** từ WDK (`samples\hid\vhf` nếu có).
2. Đổi VID/PID, usage page 0xF1D0.
3. Build Debug x64.
4. Cài bằng `devcon`.
5. Kiểm tra HIDView.

Xem `docs/05-HID-DESCRIPTOR.md`.
