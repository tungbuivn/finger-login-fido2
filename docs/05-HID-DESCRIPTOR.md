# 05 — HID descriptor / VHF

## Mục tiêu

Windows `hidfido.sys` phải bind vào top-level collection:

- Usage Page: **0xF1D0** (FIDO Alliance)
- Usage: **0x01** (U2F/FIDO)
- Input report 64 byte
- Output report 64 byte

## Descriptor mẫu (cần kiểm chứng)

Đây là hướng dẫn, **không phải descriptor đã test**:

```c
0x06, 0xD0, 0xF1,        // Usage Page (FIDO)
0x09, 0x01,              // Usage (U2F)
0xA1, 0x01,              // Collection (Application)
0x09, 0x20,              //   Usage (Input Report Data)
0x15, 0x00,              //   Logical Minimum (0)
0x26, 0xFF, 0x00,        //   Logical Maximum (255)
0x75, 0x08,              //   Report Size (8)
0x95, 0x40,              //   Report Count (64)
0x81, 0x02,              //   Input (Data,Var,Abs)
0x09, 0x21,              //   Usage (Output Report Data)
0x15, 0x00,
0x26, 0xFF, 0x00,
0x75, 0x08,
0x95, 0x40,
0x91, 0x02,              //   Output (Data,Var,Abs)
0xC0                     // End Collection
```

Có thể cần thêm report ID hoặc descriptor đúng theo FIDO HID spec.

## VHF

- Driver WDF gọi `VhfCreate`
- Cấu hình `VHF_CONFIG` với:
  - `VhfConfig.VendorID/ProductID`
  - `VhfConfig.ReportDescriptor`
  - Callback `EvtVhfReadyForNextReadReport`
- Gửi report bằng `VhfReadReportSubmit`
- Nhận output report qua queue/IOCTL

## Kiểm tra

1. Device Manager → Human Interface Devices
2. Xem Hardware IDs:
   - `HID\VID_xxxx&PID_xxxx`
3. Kiểm tra service `hidfido` đang chạy:
   ```powershell
   Get-Service hidfido
   ```
4. Dùng HIDView xem top-level collection usage page 0xF1D0.

## Nếu `hidfido` không bind

- Report descriptor sai usage page/collection
- Thiếu input/output report
- Sai report length
- Driver chưa load hoặc VHF device không start

## Ghi chú

- Không cần driver USB.
- Vẫn cần ký driver/test signing vì VHF là kernel driver.
- Windows có thể yêu cầu driver package INF/catalog; test signing cho phép.
