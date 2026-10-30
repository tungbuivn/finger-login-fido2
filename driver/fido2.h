// fido2.h - Phase 1 VHF virtual FIDO HID device.
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include <vhf.h>

#define FIDO2_VID 0x1209
#define FIDO2_PID 0xF1D0

// {7d2a6f31-9e4b-4b6e-9f6a-2f0d6a5b1c01}
DEFINE_GUID(GUID_DEVINTERFACE_FIDO2_VIRTUAL,
    0x7d2a6f31, 0x9e4b, 0x4b6e, 0x9f, 0x6a, 0x2f, 0x0d, 0x6a, 0x5b, 0x1c, 0x01);

typedef struct _DEVICE_CONTEXT {
    VHFHANDLE Vhf;
    UCHAR LastOutput[64];
    ULONG LastOutputLength;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

EVT_WDF_DRIVER_DEVICE_ADD Fido2EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP Fido2EvtDeviceCleanup;
EVT_VHF_READY_FOR_NEXT_READ_REPORT Fido2EvtVhfReady;
EVT_VHF_ASYNC_OPERATION Fido2EvtVhfWriteReport;
EVT_VHF_ASYNC_OPERATION Fido2EvtVhfGetFeature;
EVT_VHF_ASYNC_OPERATION Fido2EvtVhfSetFeature;
EVT_VHF_ASYNC_OPERATION Fido2EvtVhfGetInputReport;
EVT_VHF_CLEANUP Fido2EvtVhfCleanup;

