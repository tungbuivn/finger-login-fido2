// hidtest.cpp - send CTAPHID INIT/PING to the VHF FIDO HID device.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

static bool write_output(HANDLE h, const uint8_t* pkt, DWORD len) {
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD wrote = 0;
    BOOL ok = WriteFile(h, pkt, len, &wrote, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, 2000) == WAIT_OBJECT_0)
            ok = GetOverlappedResult(h, &ov, &wrote, FALSE);
    }
    CloseHandle(ov.hEvent);
    return ok && wrote == len;
}
static bool read_input(HANDLE h, uint8_t* pkt, DWORD timeoutMs) {
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    uint8_t tmp[65] = {};
    DWORD got = 0;
    BOOL ok = ReadFile(h, tmp, 65, &got, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, timeoutMs) == WAIT_OBJECT_0) {
            ok = GetOverlappedResult(h, &ov, &got, FALSE);
        }
    }
    CloseHandle(ov.hEvent);
    if (!ok) return false;
    if (got == 65 && tmp[0] == 0) {
        memcpy(pkt, tmp + 1, 64);
        return true;
    }
    if (got == 64) {
        memcpy(pkt, tmp, 64);
        return true;
    }
    return false;
}

int main() {
    GUID guid;
    HidD_GetHidGuid(&guid);
    HDEVINFO devs = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devs == INVALID_HANDLE_VALUE) { std::printf("SetupDi failed\n"); return 1; }

    HANDLE dev = INVALID_HANDLE_VALUE;
    for (DWORD i = 0; dev == INVALID_HANDLE_VALUE; ++i) {
        SP_DEVICE_INTERFACE_DATA did = { sizeof(did) };
        if (!SetupDiEnumDeviceInterfaces(devs, nullptr, &guid, i, &did)) break;
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(devs, &did, nullptr, 0, &need, nullptr);
        if (!need) continue;
        auto* detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(need);
        detail->cbSize = sizeof(*detail);
        if (SetupDiGetDeviceInterfaceDetailW(devs, &did, detail, need, nullptr, nullptr)) {
            HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES a = { sizeof(a) };
                if (HidD_GetAttributes(h, &a)) {
                    std::printf("HID %04X:%04X %ls\n", a.VendorID, a.ProductID, detail->DevicePath);
                }
                if (a.VendorID == 0x1209 && a.ProductID == 0xF1D0) {
                    wprintf(L"FIDO HID: %s\n", detail->DevicePath);
                    dev = h;
                } else {
                    CloseHandle(h);
                }
            }
        }
        free(detail);
    }
    SetupDiDestroyDeviceInfoList(devs);
    if (dev == INVALID_HANDLE_VALUE) { std::printf("FIDO HID not found\n"); return 2; }

    HidD_SetNumInputBuffers(dev, 8);

    uint8_t init[64] = {};
    init[0] = 0xFF; init[1] = 0xFF; init[2] = 0xFF; init[3] = 0xFF;
    init[4] = 0x86;
    init[5] = 0x00; init[6] = 0x08;
    for (int i = 0; i < 8; ++i) init[7 + i] = (uint8_t)(0xA0 + i);

    uint8_t outbuf[65] = {};
    std::memcpy(outbuf + 1, init, 64);
    if (!write_output(dev, outbuf, 65)) {
        std::printf("WriteFile init failed: %lu\n", GetLastError());
        CloseHandle(dev);
        return 3;
    }
    uint8_t in[64] = {};
    if (!read_input(dev, in, 3000)) {
        std::printf("INIT read failed: %lu\n", GetLastError());
        CloseHandle(dev);
        return 4;
    }
    std::printf("INIT response:");
    for (int i = 0; i < 32; ++i) std::printf(" %02X", in[i]);
    std::printf("\n");
    if (in[4] != 0x86) { CloseHandle(dev); return 5; }

    uint32_t cid = ((uint32_t)in[15] << 24) | ((uint32_t)in[16] << 16) |
                   ((uint32_t)in[17] << 8) | in[18];
    std::printf("assigned CID=%08X\n", cid);

    uint8_t ping[64] = {};
    ping[0] = (uint8_t)(cid >> 24); ping[1] = (uint8_t)(cid >> 16);
    ping[2] = (uint8_t)(cid >> 8); ping[3] = (uint8_t)cid;
    ping[4] = 0x81;
    const char* msg = "hello";
    ping[5] = 0x00; ping[6] = 0x05;
    std::memcpy(ping + 7, msg, 5);
    std::memset(outbuf, 0, sizeof(outbuf));
    std::memcpy(outbuf + 1, ping, 64);
    if (!write_output(dev, outbuf, 65)) {
        std::printf("WriteFile ping failed: %lu\n", GetLastError());
        CloseHandle(dev);
        return 6;
    }
    if (!read_input(dev, in, 3000)) {
        std::printf("PING read failed: %lu\n", GetLastError());
        CloseHandle(dev);
        return 7;
    }
    std::printf("PING response:");
    for (int i = 0; i < 16; ++i) std::printf(" %02X", in[i]);
    std::printf("\n");

    uint8_t info[64] = {};
    info[0] = (uint8_t)(cid >> 24); info[1] = (uint8_t)(cid >> 16);
    info[2] = (uint8_t)(cid >> 8); info[3] = (uint8_t)cid;
    info[4] = 0x90; info[5] = 0x00; info[6] = 0x01; info[7] = 0x04;
    std::memset(outbuf, 0, sizeof(outbuf));
    std::memcpy(outbuf + 1, info, 64);
    if (!write_output(dev, outbuf, 65)) { std::printf("getInfo write failed: %lu\n", GetLastError()); CloseHandle(dev); return 9; }
    if (!read_input(dev, in, 3000)) { std::printf("getInfo read failed: %lu\n", GetLastError()); CloseHandle(dev); return 10; }
    std::printf("getInfo response:");
    for (int i = 0; i < 32; ++i) std::printf(" %02X", in[i]);
    std::printf("\n");
    CloseHandle(dev);
    return (in[4] == 0x90 && in[7] == 0x00) ? 0 : 8;
}
