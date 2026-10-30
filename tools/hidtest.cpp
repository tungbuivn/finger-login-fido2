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
#include <thread>
#include <atomic>
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

static bool send_msg(HANDLE dev, uint32_t cid, uint8_t cmd, const uint8_t* data, size_t len) {
    uint8_t pkt[64] = {};
    uint8_t out[65] = {};
    pkt[0] = (uint8_t)(cid >> 24); pkt[1] = (uint8_t)(cid >> 16);
    pkt[2] = (uint8_t)(cid >> 8); pkt[3] = (uint8_t)cid;
    pkt[4] = cmd; pkt[5] = (uint8_t)(len >> 8); pkt[6] = (uint8_t)len;
    size_t first = len < 57 ? len : 57;
    std::memcpy(pkt + 7, data, first);
    std::memcpy(out + 1, pkt, 64);
    if (!write_output(dev, out, 65)) return false;
    Sleep(200);
    size_t off = first;
    uint8_t seq = 0;
    while (off < len) {
        std::memset(pkt, 0, sizeof(pkt));
        pkt[0] = (uint8_t)(cid >> 24); pkt[1] = (uint8_t)(cid >> 16);
        pkt[2] = (uint8_t)(cid >> 8); pkt[3] = (uint8_t)cid;
        pkt[4] = seq++;
        size_t n = len - off < 59 ? len - off : 59;
        std::memcpy(pkt + 5, data + off, n);
        off += n;
        std::memset(out, 0, sizeof(out));
        std::memcpy(out + 1, pkt, 64);
        if (!write_output(dev, out, 65)) return false;
        Sleep(200);
    }
    return true;
}

static int recv_msg(HANDLE dev, uint32_t cid, uint8_t cmd, uint8_t* out, int maxlen) {
    uint8_t pkt[64] = {};
    if (!read_input(dev, pkt, 3000)) return -1;
    uint32_t rcid = ((uint32_t)pkt[0] << 24) | ((uint32_t)pkt[1] << 16) |
                    ((uint32_t)pkt[2] << 8) | pkt[3];
    std::printf("recv raw cid=%08X cmd=%02X len=%u\\n", rcid, pkt[4], (pkt[5] << 8) | pkt[6]);
    if (rcid != cid || pkt[4] != cmd) return -2;
    int len = (pkt[5] << 8) | pkt[6];
    int got = len < 57 ? len : 57;
    if (len > maxlen) return -3;
    std::memcpy(out, pkt + 7, got);
    uint8_t seq = 0;
    while (got < len) {
        if (!read_input(dev, pkt, 3000)) return -4;
        if (pkt[4] != seq) return -5;
        seq++;
        int n = len - got < 59 ? len - got : 59;
        std::memcpy(out + got, pkt + 5, n);
        got += n;
    }
    return len;
}

static const uint8_t k_make[] = {
0x01,0xA4,0x01,0x58,0x20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0x02,0xA2,0x62,0x69,0x64,0x6B,0x65,0x78,0x61,0x6D,0x70,0x6C,0x65,0x2E,0x63,0x6F,0x6D,
0x64,0x6E,0x61,0x6D,0x65,0x67,0x45,0x78,0x61,0x6D,0x70,0x6C,0x65,
0x03,0xA3,0x62,0x69,0x64,0x44,0x74,0x65,0x73,0x74,0x64,0x6E,0x61,0x6D,0x65,0x64,0x74,0x65,0x73,0x74,
0x6B,0x64,0x69,0x73,0x70,0x6C,0x61,0x79,0x4E,0x61,0x6D,0x65,0x64,0x54,0x65,0x73,0x74,
0x04,0x81,0xA2,0x64,0x74,0x79,0x70,0x65,0x6A,0x70,0x75,0x62,0x6C,0x69,0x63,0x2D,0x6B,0x65,0x79,0x63,0x61,0x6C,0x67,0x26
};
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

    uint8_t info[1] = { 0x04 };
    if (!send_msg(dev, cid, 0x90, info, sizeof(info))) {
        std::printf("getInfo send failed\n"); CloseHandle(dev); return 9;
    }
    uint8_t resp[2048] = {};
    int glen = recv_msg(dev, cid, 0x90, resp, sizeof(resp));
    std::printf("getInfo response len=%d status=%02X\n", glen, glen > 0 ? resp[0] : 0xFF);
    for (int i = 0; i < (glen > 0 ? glen : 0); ++i) std::printf(" %02X", resp[i]);
    std::printf("\n");
    while (read_input(dev, resp, 50)) {}
    uint8_t first[64] = {};
    std::atomic<bool> gotFirst{false};
    std::thread reader([&]() {
        if (read_input(dev, first, 5000)) gotFirst = true;
    });
    Sleep(20);
    if (!send_msg(dev, cid, 0x90, k_make, sizeof(k_make))) {
        std::printf("makeCredential send failed\n"); reader.join(); CloseHandle(dev); return 11;
    }
    reader.join();
    std::printf("make first=%d cid=%08X cmd=%02X len=%u\n", gotFirst ? 1 : 0,
                ((uint32_t)first[0] << 24) | ((uint32_t)first[1] << 16) | ((uint32_t)first[2] << 8) | first[3],
                first[4], (first[5] << 8) | first[6]);
    int rlen = recv_msg(dev, cid, 0x90, resp, sizeof(resp));
    std::printf("makeCredential continuation len=%d status=%02X\n", rlen, rlen > 0 ? resp[0] : 0xFF);

    CloseHandle(dev);
    return (rlen > 0 && resp[0] == 0x00) ? 0 : 8;
}
