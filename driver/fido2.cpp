// fido2.c - Phase 1: virtual FIDO HID device via VHF.
#include <initguid.h>
#include "fido2.h"

// FIDO HID descriptor: Usage Page 0xF1D0, Usage 0x01, 64-byte input/output.
static const UCHAR g_ReportDescriptor[] = {
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
};

static VOID Complete(PVOID context, VHFOPERATIONHANDLE op) {
    UNREFERENCED_PARAMETER(context);
    VhfAsyncOperationComplete(op, STATUS_SUCCESS);
}

VOID Fido2EvtVhfWriteReport(PVOID VhfClientContext, VHFOPERATIONHANDLE VhfOperationHandle, PVOID VhfOperationContext, PHID_XFER_PACKET HidTransferPacket) {
    UNREFERENCED_PARAMETER(VhfOperationContext);
    UNREFERENCED_PARAMETER(VhfOperationHandle);
    PDEVICE_CONTEXT ctx = static_cast<PDEVICE_CONTEXT>(VhfClientContext);
    if (ctx && HidTransferPacket && HidTransferPacket->reportBuffer) {
        ULONG n = HidTransferPacket->reportBufferLen;
        if (n > sizeof(ctx->LastOutput)) n = sizeof(ctx->LastOutput);
        RtlCopyMemory(ctx->LastOutput, HidTransferPacket->reportBuffer, n);
        ctx->LastOutputLength = n;
    }
    VhfAsyncOperationComplete(VhfOperationHandle, STATUS_SUCCESS);
}

VOID Fido2EvtVhfGetFeature(PVOID VhfClientContext, VHFOPERATIONHANDLE VhfOperationHandle, PVOID VhfOperationContext, PHID_XFER_PACKET HidTransferPacket) {
    UNREFERENCED_PARAMETER(VhfClientContext);
    UNREFERENCED_PARAMETER(VhfOperationContext);
    if (HidTransferPacket) HidTransferPacket->reportBufferLen = 0;
    VhfAsyncOperationComplete(VhfOperationHandle, STATUS_SUCCESS);
}

VOID Fido2EvtVhfSetFeature(PVOID VhfClientContext, VHFOPERATIONHANDLE VhfOperationHandle, PVOID VhfOperationContext, PHID_XFER_PACKET HidTransferPacket) {
    UNREFERENCED_PARAMETER(VhfClientContext);
    UNREFERENCED_PARAMETER(VhfOperationContext);
    UNREFERENCED_PARAMETER(HidTransferPacket);
    VhfAsyncOperationComplete(VhfOperationHandle, STATUS_SUCCESS);
}

VOID Fido2EvtVhfGetInputReport(PVOID VhfClientContext, VHFOPERATIONHANDLE VhfOperationHandle, PVOID VhfOperationContext, PHID_XFER_PACKET HidTransferPacket) {
    UNREFERENCED_PARAMETER(VhfClientContext);
    UNREFERENCED_PARAMETER(VhfOperationContext);
    if (HidTransferPacket) HidTransferPacket->reportBufferLen = 0;
    VhfAsyncOperationComplete(VhfOperationHandle, STATUS_SUCCESS);
}

VOID Fido2EvtVhfReady(PVOID VhfClientContext) {
    UNREFERENCED_PARAMETER(VhfClientContext);
    // Phase 1: khong co du lieu; service se gui report sau.
}

VOID Fido2EvtVhfCleanup(PVOID VhfClientContext) {
    UNREFERENCED_PARAMETER(VhfClientContext);
}

VOID Fido2EvtDeviceCleanup(WDFOBJECT CleanupContext) {
    PDEVICE_CONTEXT ctx = DeviceGetContext(CleanupContext);
    if (ctx->Vhf) {
        VhfDelete(ctx->Vhf, TRUE);
        ctx->Vhf = nullptr;
    }
}

NTSTATUS Fido2EvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

    WDFDEVICE device = nullptr;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DEVICE_CONTEXT);
    attrs.EvtCleanupCallback = Fido2EvtDeviceCleanup;
    NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attrs, &device);
    if (!NT_SUCCESS(status)) return status;

    PDEVICE_CONTEXT ctx = DeviceGetContext(device);
    RtlZeroMemory(ctx, sizeof(*ctx));

    VHF_CONFIG cfg;
    VHF_CONFIG_INIT(&cfg, WdfDeviceWdmGetDeviceObject(device),
                    sizeof(g_ReportDescriptor), (PUCHAR)g_ReportDescriptor);
    cfg.VendorID = FIDO2_VID;
    cfg.ProductID = FIDO2_PID;
    cfg.VersionNumber = 0x0100;
    cfg.VhfClientContext = ctx;
    cfg.EvtVhfAsyncOperationWriteReport = Fido2EvtVhfWriteReport;
    cfg.EvtVhfAsyncOperationGetFeature = Fido2EvtVhfGetFeature;
    cfg.EvtVhfAsyncOperationSetFeature = Fido2EvtVhfSetFeature;
    cfg.EvtVhfAsyncOperationGetInputReport = Fido2EvtVhfGetInputReport;
    cfg.EvtVhfReadyForNextReadReport = Fido2EvtVhfReady;
    cfg.EvtVhfCleanup = Fido2EvtVhfCleanup;

    status = VhfCreate(&cfg, &ctx->Vhf);
    if (!NT_SUCCESS(status)) {
        ctx->Vhf = nullptr;
        return status;
    }
    status = VhfStart(ctx->Vhf);
    if (!NT_SUCCESS(status)) {
        VhfDelete(ctx->Vhf, TRUE);
        ctx->Vhf = nullptr;
        return status;
    }

    return WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_FIDO2_VIRTUAL, nullptr);
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, Fido2EvtDeviceAdd);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
