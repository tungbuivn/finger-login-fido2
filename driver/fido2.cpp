// fido2.cpp - WDM function driver for a root PDO, VHF virtual FIDO HID.
#include <initguid.h>
#include <ntddk.h>
#include <hidport.h>
#include <vhf.h>
#include <ntstrsafe.h>

#define FIDO2_VID 0x1209
#define FIDO2_PID 0xF1D0

DEFINE_GUID(GUID_DEVINTERFACE_FIDO2_VIRTUAL,
    0x7d2a6f31, 0x9e4b, 0x4b6e, 0x9f, 0x6a, 0x2f, 0x0d, 0x6a, 0x5b, 0x1c, 0x01);

typedef struct _DEVICE_CONTEXT {
    VHFHANDLE Vhf;
    PDEVICE_OBJECT Lower;
    PDEVICE_OBJECT Pdo;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

#define FIDO2_IOCTL_GET_OUTPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
#define FIDO2_IOCTL_SEND_INPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

static PDEVICE_OBJECT g_ControlDevice = nullptr;
static PDEVICE_CONTEXT g_Ctx = nullptr;
static KSPIN_LOCK g_Lock;
static UCHAR g_Queue[16][64];
static ULONG g_Head = 0;
static ULONG g_Tail = 0;

static const UCHAR g_ReportDescriptor[] = {
    0x06, 0xD0, 0xF1,
    0x09, 0x01,
    0xA1, 0x01,
    0x09, 0x20,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x40,
    0x81, 0x02,
    0x09, 0x21,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x40,
    0x91, 0x02,
    0xC0};

static void FileLog(const char* text) {
    UNICODE_STRING path;
    RtlInitUnicodeString(&path, L"\\??\\C:\\fido2.log");
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               nullptr, nullptr);
    IO_STATUS_BLOCK iosb;
    HANDLE h = nullptr;
    NTSTATUS st = ZwCreateFile(&h, FILE_APPEND_DATA | SYNCHRONIZE, &oa, &iosb,
                               nullptr, FILE_ATTRIBUTE_NORMAL,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT,
                               nullptr, 0);
    if (NT_SUCCESS(st)) {
        ZwWriteFile(h, nullptr, nullptr, nullptr, &iosb,
                    (PVOID)text, (ULONG)strlen(text), nullptr, nullptr);
        ZwClose(h);
    }
}

static void Stage(const char* name, NTSTATUS st) {
    char b[96];
    RtlStringCchPrintfA(b, sizeof(b), "%s=%08X\r\n", name, (unsigned)st);
    FileLog(b);
}

VOID EvtVhfReady(PVOID ctx) { UNREFERENCED_PARAMETER(ctx); }
VOID EvtVhfWrite(PVOID ctx, VHFOPERATIONHANDLE op, PVOID opCtx, PHID_XFER_PACKET pkt) {
    UNREFERENCED_PARAMETER(ctx); UNREFERENCED_PARAMETER(opCtx);
    if (pkt && pkt->reportBuffer && pkt->reportBufferLen >= 64) {
        const UCHAR* p = (const UCHAR*)pkt->reportBuffer;
        ULONG n = pkt->reportBufferLen;
        if (n == 65 && p[0] == 0) { p++; n = 64; }
        KIRQL old;
        KeAcquireSpinLock(&g_Lock, &old);
        ULONG next = (g_Head + 1) % 16;
        if (next != g_Tail) {
            RtlZeroMemory(g_Queue[g_Head], 64);
            RtlCopyMemory(g_Queue[g_Head], p, n < 64 ? n : 64);
            g_Head = next;
        }
        KeReleaseSpinLock(&g_Lock, old);
    }
    VhfAsyncOperationComplete(op, STATUS_SUCCESS);
}
VOID EvtVhfGetFeature(PVOID ctx, VHFOPERATIONHANDLE op, PVOID opCtx, PHID_XFER_PACKET pkt) {
    UNREFERENCED_PARAMETER(ctx); UNREFERENCED_PARAMETER(opCtx);
    if (pkt) pkt->reportBufferLen = 0;
    VhfAsyncOperationComplete(op, STATUS_SUCCESS);
}
VOID EvtVhfSetFeature(PVOID ctx, VHFOPERATIONHANDLE op, PVOID opCtx, PHID_XFER_PACKET pkt) {
    UNREFERENCED_PARAMETER(ctx); UNREFERENCED_PARAMETER(opCtx); UNREFERENCED_PARAMETER(pkt);
    VhfAsyncOperationComplete(op, STATUS_SUCCESS);
}
VOID EvtVhfGetInput(PVOID ctx, VHFOPERATIONHANDLE op, PVOID opCtx, PHID_XFER_PACKET pkt) {
    UNREFERENCED_PARAMETER(ctx); UNREFERENCED_PARAMETER(opCtx);
    if (pkt) pkt->reportBufferLen = 0;
    VhfAsyncOperationComplete(op, STATUS_SUCCESS);
}
VOID EvtVhfCleanup(PVOID ctx) { UNREFERENCED_PARAMETER(ctx); }

static NTSTATUS DispatchPnp(PDEVICE_OBJECT dev, PIRP irp) {
    auto* ctx = (PDEVICE_CONTEXT)dev->DeviceExtension;
    if (!ctx->Lower) {
        irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }
    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(ctx->Lower, irp);
}

static NTSTATUS DispatchPower(PDEVICE_OBJECT dev, PIRP irp) {
    auto* ctx = (PDEVICE_CONTEXT)dev->DeviceExtension;
    PoStartNextPowerIrp(irp);
    IoSkipCurrentIrpStackLocation(irp);
    return PoCallDriver(ctx->Lower, irp);
}

static NTSTATUS DispatchPass(PDEVICE_OBJECT dev, PIRP irp) {
    auto* ctx = (PDEVICE_CONTEXT)dev->DeviceExtension;
    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(ctx->Lower, irp);
}

static NTSTATUS DispatchControl(PDEVICE_OBJECT dev, PIRP irp) {
    UNREFERENCED_PARAMETER(dev);
    auto* stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG info = 0;

    if (stack->MajorFunction == IRP_MJ_CREATE || stack->MajorFunction == IRP_MJ_CLOSE) {
        status = STATUS_SUCCESS;
    } else if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
        if (!g_Ctx) {
            status = STATUS_DEVICE_NOT_READY;
        } else if (code == FIDO2_IOCTL_GET_OUTPUT) {
            if (stack->Parameters.DeviceIoControl.OutputBufferLength < 64) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                KIRQL old;
                KeAcquireSpinLock(&g_Lock, &old);
                if (g_Head == g_Tail) {
                    status = STATUS_NO_MORE_ENTRIES;
                } else {
                    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, g_Queue[g_Tail], 64);
                    g_Tail = (g_Tail + 1) % 16;
                    info = 64;
                    status = STATUS_SUCCESS;
                }
                KeReleaseSpinLock(&g_Lock, old);
            }
        } else if (code == FIDO2_IOCTL_SEND_INPUT) {
            if (stack->Parameters.DeviceIoControl.InputBufferLength < 64) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                HID_XFER_PACKET pkt = {};
                pkt.reportBuffer = (PUCHAR)irp->AssociatedIrp.SystemBuffer;
                pkt.reportBufferLen = 64;
                pkt.reportId = 0;
                status = VhfReadReportSubmit(g_Ctx->Vhf, &pkt);
            }
        }
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS AddDevice(PDRIVER_OBJECT driver, PDEVICE_OBJECT pdo) {
    FileLog("adddevice\r\n");
    PDEVICE_OBJECT fdo = nullptr;
    NTSTATUS status = IoCreateDevice(driver, sizeof(DEVICE_CONTEXT), nullptr,
                                     FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
                                     FALSE, &fdo);
    Stage("fdo", status);
    if (!NT_SUCCESS(status)) return status;

    auto* ctx = (PDEVICE_CONTEXT)fdo->DeviceExtension;
    RtlZeroMemory(ctx, sizeof(*ctx));
    ctx->Pdo = pdo;
    ctx->Lower = IoAttachDeviceToDeviceStack(fdo, pdo);
    Stage("attach", ctx->Lower ? STATUS_SUCCESS : STATUS_NO_SUCH_DEVICE);
    if (!ctx->Lower) {
        IoDeleteDevice(fdo);
        return STATUS_NO_SUCH_DEVICE;
    }
    fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    static WCHAR instanceId[] = L"FIDO2Virtual";
    GUID containerId = {0x7d2a6f31,0x9e4b,0x4b6e,{0x9f,0x6a,0x2f,0x0d,0x6a,0x5b,0x1c,0x01}};
    VHF_CONFIG cfg;
    VHF_CONFIG_INIT(&cfg, fdo, sizeof(g_ReportDescriptor), (PUCHAR)g_ReportDescriptor);
    cfg.InstanceID = instanceId;
    cfg.InstanceIDLength = (USHORT)(wcslen(instanceId) * sizeof(WCHAR));
    cfg.ContainerID = containerId;
    cfg.VendorID = FIDO2_VID;
    cfg.ProductID = FIDO2_PID;
    cfg.VersionNumber = 0x0100;
    cfg.VhfClientContext = ctx;
    cfg.EvtVhfReadyForNextReadReport = EvtVhfReady;
    cfg.EvtVhfAsyncOperationWriteReport = EvtVhfWrite;
    cfg.EvtVhfAsyncOperationGetFeature = EvtVhfGetFeature;
    cfg.EvtVhfAsyncOperationSetFeature = EvtVhfSetFeature;
    cfg.EvtVhfAsyncOperationGetInputReport = EvtVhfGetInput;
    cfg.EvtVhfCleanup = EvtVhfCleanup;

    status = VhfCreate(&cfg, &ctx->Vhf);
    Stage("vhfcreate", status);
    if (!NT_SUCCESS(status)) {
        IoDetachDevice(ctx->Lower);
        IoDeleteDevice(fdo);
        return status;
    }
    status = VhfStart(ctx->Vhf);
    Stage("vhfstart", status);
    if (!NT_SUCCESS(status)) {
        VhfDelete(ctx->Vhf, TRUE);
        ctx->Vhf = nullptr;
        IoDetachDevice(ctx->Lower);
        IoDeleteDevice(fdo);
        return status;
    }
    FileLog("ready\r\n");
    g_Ctx = ctx;
    return STATUS_SUCCESS;
}

static VOID DriverUnload(PDRIVER_OBJECT driver) {
    FileLog("unload\r\n");
    g_Ctx = nullptr;
    if (g_ControlDevice) {
        UNICODE_STRING symName;
        RtlInitUnicodeString(&symName, L"\\DosDevices\\Fido2Ctl");
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(g_ControlDevice);
        g_ControlDevice = nullptr;
    }
    PDEVICE_OBJECT dev = driver->DeviceObject;
    while (dev) {
        PDEVICE_OBJECT next = dev->NextDevice;
        auto* ctx = (PDEVICE_CONTEXT)dev->DeviceExtension;
        if (ctx->Vhf) {
            VhfDelete(ctx->Vhf, TRUE);
            ctx->Vhf = nullptr;
        }
        if (ctx->Lower) {
            IoDetachDevice(ctx->Lower);
            ctx->Lower = nullptr;
        }
        IoDeleteDevice(dev);
        dev = next;
    }
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);
    FileLog("entry\r\n");
    KeInitializeSpinLock(&g_Lock);
    driver->DriverUnload = DriverUnload;
    driver->DriverExtension->AddDevice = AddDevice;
    driver->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
    driver->MajorFunction[IRP_MJ_POWER] = DispatchPower;
    driver->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = DispatchPass;
    driver->MajorFunction[IRP_MJ_CREATE] = DispatchControl;
    driver->MajorFunction[IRP_MJ_CLOSE] = DispatchControl;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;

    UNICODE_STRING devName, symName;
    RtlInitUnicodeString(&devName, L"\\Device\\Fido2Ctl");
    RtlInitUnicodeString(&symName, L"\\DosDevices\\Fido2Ctl");
    NTSTATUS status = IoCreateDevice(driver, 0, &devName, FILE_DEVICE_UNKNOWN,
                                     FILE_DEVICE_SECURE_OPEN, FALSE, &g_ControlDevice);
    Stage("ctldev", status);
    if (NT_SUCCESS(status)) {
        g_ControlDevice->Flags |= DO_BUFFERED_IO;
        status = IoCreateSymbolicLink(&symName, &devName);
        Stage("ctlsym", status);
    }
    return NT_SUCCESS(status) ? STATUS_SUCCESS : status;
}
