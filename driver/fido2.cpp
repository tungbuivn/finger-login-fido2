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
    UNREFERENCED_PARAMETER(ctx); UNREFERENCED_PARAMETER(opCtx); UNREFERENCED_PARAMETER(pkt);
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
    return STATUS_SUCCESS;
}

static VOID DriverUnload(PDRIVER_OBJECT driver) {
    FileLog("unload\r\n");
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
    driver->DriverUnload = DriverUnload;
    driver->DriverExtension->AddDevice = AddDevice;
    driver->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
    driver->MajorFunction[IRP_MJ_POWER] = DispatchPower;
    driver->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = DispatchPass;
    return STATUS_SUCCESS;
}
