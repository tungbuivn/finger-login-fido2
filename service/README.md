# service/

Chưa có code. Kế hoạch:

```text
service/
  CMakeLists.txt
  main.cpp
  hid_transport.cpp
  ctaphid.cpp
  ctap2.cpp
  cbor.cpp
  credential_store.cpp
  dpapi.cpp
```

## Trách nhiệm

- Mở device interface của driver
- Đọc/ghi HID report
- CTAPHID framing
- CTAP2 CBOR
- Credential store DPAPI
- Log

## MVP

- `getInfo`
- `makeCredential` (ES256, attestation none)
- `getAssertion`
- Chưa PIN/UV

## Build dự kiến

```powershell
cmake -S service -B service/build -A x64
cmake --build service/build --config Debug
```

Xem `docs/04-CTAP2-NOTES.md`.
