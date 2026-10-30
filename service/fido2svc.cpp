// fido2svc.cpp - user-mode CTAPHID transport for the FIDO2 VHF device.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "ctap2.h"

#define FIDO2_IOCTL_GET_OUTPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
#define FIDO2_IOCTL_SEND_INPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

static HANDLE g_ctl = INVALID_HANDLE_VALUE;
static uint32_t g_cid = 1;
static uint8_t g_in[7600];
static size_t g_inLen = 0;
static uint8_t g_inCmd = 0;
static uint32_t g_inCid = 0;
static size_t g_inExpected = 0;
static uint8_t g_nextSeq = 0;

static void dump(const char* tag, const uint8_t* p, size_t n) {
    std::printf("%s (%zu):", tag, n);
    for (size_t i = 0; i < n && i < 32; ++i) std::printf(" %02X", p[i]);
    std::printf("\n");
}

static bool send_one(const uint8_t* pkt) {
    for (int i = 0; i < 200; ++i) {
        DWORD ret = 0;
        if (DeviceIoControl(g_ctl, FIDO2_IOCTL_SEND_INPUT, (LPVOID)pkt, 64,
                            nullptr, 0, &ret, nullptr))
            return true;
        DWORD err = GetLastError();
        if (err != ERROR_BUSY && err != ERROR_RETRY) {
            std::printf("SEND input failed: %lu\n", err);
            return false;
        }
        Sleep(5);
    }
    std::printf("SEND input busy timeout\n");
    return false;
}

static bool send_frame(uint32_t cid, uint8_t cmd, const uint8_t* data, size_t len) {
    uint8_t pkt[64];
    std::memset(pkt, 0, sizeof(pkt));
    pkt[0] = (uint8_t)(cid >> 24); pkt[1] = (uint8_t)(cid >> 16);
    pkt[2] = (uint8_t)(cid >> 8);  pkt[3] = (uint8_t)cid;
    pkt[4] = cmd;
    pkt[5] = (uint8_t)(len >> 8); pkt[6] = (uint8_t)len;
    size_t first = len < 57 ? len : 57;
    std::memcpy(pkt + 7, data, first);
    if (!send_one(pkt)) return false;
    size_t off = first;
    uint8_t seq = 0;
    while (off < len) {
        std::memset(pkt, 0, sizeof(pkt));
        pkt[0] = (uint8_t)(cid >> 24); pkt[1] = (uint8_t)(cid >> 16);
        pkt[2] = (uint8_t)(cid >> 8);  pkt[3] = (uint8_t)cid;
        pkt[4] = seq++;
        size_t n = len - off < 59 ? len - off : 59;
        std::memcpy(pkt + 5, data + off, n);
        off += n;
        if (!send_one(pkt)) return false;
    }
    return true;
}
static void handle_cbor(uint32_t cid, const uint8_t* data, size_t len) {
    if (len < 1) return;
    dump("CTAP2", data, len);
    auto out = ctap2_handle(data[0], data + 1, len - 1);
    std::printf("CTAP2 out len=%zu first=%02X\n", out.size(), out.empty() ? 0 : out[0]);
    bool ok = send_frame(cid, 0x90, out.data(), out.size());
    std::printf("CTAP2 send=%d\n", ok ? 1 : 0);
}
static void handle_message(uint32_t cid, uint8_t cmd, const uint8_t* data, size_t len) {
    dump("HOST", data, len);
    switch (cmd) {
    case 0x86: { // CTAPHID_INIT
        uint8_t out[17] = {};
        if (len >= 8) std::memcpy(out, data, 8);
        out[8] = 0; out[9] = 0; out[10] = 0; out[11] = 1;
        out[12] = 2; // CTAPHID protocol
        out[13] = 1; out[14] = 0; out[15] = 0; // device version
        out[16] = 0x04; // CBOR capability
        send_frame(cid, 0x86, out, sizeof(out));
        break;
    }
    case 0x81: // CTAPHID_PING
        send_frame(cid, 0x81, data, len);
        break;
    case 0x90: // CTAPHID_CBOR
        handle_cbor(cid, data, len);
        break;
    default: {
        uint8_t err = 0x01; // CTAPHID_ERROR invalid command
        send_frame(cid, 0xBF, &err, 1);
        break;
    }
    }
}

static void process_packet(const uint8_t* pkt) {
    uint32_t cid = ((uint32_t)pkt[0] << 24) | ((uint32_t)pkt[1] << 16) |
                   ((uint32_t)pkt[2] << 8) | pkt[3];
    uint8_t cmd = pkt[4];
    std::printf("PKT cid=%08X cmd=%02X len=%u raw=%02X %02X %02X %02X %02X %02X %02X\n", cid, cmd,
                (unsigned)(((unsigned)pkt[5] << 8) | pkt[6]),
                pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5], pkt[6]);

    if (cmd & 0x80) {
        g_inCid = cid;
        g_inCmd = cmd;
        g_inExpected = ((size_t)pkt[5] << 8) | pkt[6];
        if (g_inExpected > sizeof(g_in)) g_inExpected = sizeof(g_in);
        g_inLen = g_inExpected < 57 ? g_inExpected : 57;
        std::memcpy(g_in, pkt + 7, g_inLen);
        g_nextSeq = 0;
    } else {
        if (cid != g_inCid || g_nextSeq == 0xFF) return;
        if (pkt[4] != g_nextSeq) {
            std::printf("bad seq %u expected %u\n", pkt[4], g_nextSeq);
            return;
        }
        g_nextSeq++;
        size_t n = g_inExpected - g_inLen;
        if (n > 59) n = 59;
        std::memcpy(g_in + g_inLen, pkt + 5, n);
        g_inLen += n;
    }

    if (g_inLen >= g_inExpected) {
        handle_message(g_inCid, g_inCmd, g_in, g_inExpected);
        g_inLen = 0;
        g_inExpected = 0;
        g_inCmd = 0;
    }
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("FIDO2 CTAPHID service\n");
    g_ctl = CreateFileW(L"\\\\.\\Fido2Ctl", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (g_ctl == INVALID_HANDLE_VALUE) {
        std::printf("open \\\\.\\Fido2Ctl failed: %lu\n", GetLastError());
        return 1;
    }
    std::printf("control device open\n");

    uint8_t pkt[64];
    while (true) {
        DWORD ret = 0;
        if (DeviceIoControl(g_ctl, FIDO2_IOCTL_GET_OUTPUT, nullptr, 0, pkt, 64, &ret, nullptr)) {
            if (ret == 64) process_packet(pkt);
        } else {
            DWORD err = GetLastError();
            if (err != ERROR_NO_MORE_ITEMS) {
                // STATUS_NO_MORE_ENTRIES maps to ERROR_NO_MORE_ITEMS usually.
                Sleep(2);
            }
        }
        Sleep(2);
    }
}
