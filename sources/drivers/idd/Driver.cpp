// Copyright (C) Microsoft Corporation
// Copyright (C) 2022-2023 Intel Corporation
//
// This file contains modifications made to the original Microsoft sample code from
// https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay/IddSampleDriver
//
// List of modifications:
// 1. Customize list of supported resolutions and modes
// 2. Customize supported registry key values
// 3. Add multi-adapter support
// 4. Add support for hw cursor

#include "Driver.h"
#include "Driver.tmh"

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

#pragma region SampleMonitors

//
// This device Interface Guid for IddSampleDriver device class.
//    is used in SetupDiEnumDeviceInterfaces to enumerates all registered interfaces
//
const GUID GUID_DEVINTERFACE_IDD_DEVICE = \
{
    0x881EF630,
    0x82B2,
    0x81d2,
    { 0x88, 0x82,  0x80,  0x80,  0x8E,  0x8F,  0x82,  0x82 }
};

// If monitor count > ARRAYSIZE(s_SampleMonitors), we create edid-less monitors
static constexpr DWORD IDD_SAMPLE_MONITOR_COUNT = 2;

ULONG MonitorNumberRegistryValue = 0; // Will be used to get the number of monitors
ULONG MonitorCursorRegistryValue = 0; // Will be used to set software or hardware cursor.
ULONG AdapterLUIDLowPart         = 0; // Will be used to set the preferred render adapter.
WDFDEVICE g_Device               = nullptr;

struct MonitorMode {
    DWORD width;
    DWORD height;
    DWORD vsync;
};

// List of modes supported by this driver. Note that since there is no physical
// monitor attached we don't need EDID, so driver works in EDID-less mode.
static const MonitorMode g_modes[] =
{
    { 3840, 2160, 60 },
    { 3200, 2400, 60 },
    { 3200, 1800, 60 },
    { 3008, 1692, 60 },
    { 2880, 1800, 60 },
    { 2880, 1620, 60 },
    { 2560, 1440, 144 },
    { 2560, 1440, 90 },
    { 2048, 1536, 60 },
    { 2560, 1440, 60 },
    { 2560, 1600, 60 },
    { 2048, 1536, 60 },
    { 1920, 1440, 60 },
    { 1920, 1200, 60 },
    { 1920, 1080, 144 },
    { 1920, 1080, 90 },
    { 1920, 1080, 60 },
    { 1680, 1050, 60 },
    { 1600, 1024, 60 },
    { 1600, 900, 60 },
    { 1400, 1050,60 },
    { 1440, 900, 60 },
    { 1366, 768, 60 },
    { 1360, 768, 60 },
    { 1280, 1024, 60 },
    { 1280, 960, 60 },
    { 1280, 800, 60 },
    { 1024, 768, 75 },
    { 1280, 768, 60 },
    { 1280, 720, 60 },
    { 1280, 600, 60 },
    { 1152, 864, 60 },
    { 800, 600, 60 },
    { 640, 480, 60 },
};
// That's default mode for the driver. While we can expose any mode here as
// default, we don't want to set too high default resolution to ease initial
// system configuration.
static const MonitorMode g_default_mode = { 1920, 1080, 60 };

#pragma endregion

#pragma region helpers

static inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, DWORD Width, DWORD Height, DWORD VSync, bool bMonitorMode)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;

    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
    Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
    Mode.AdditionalSignalInfo.videoStandard    = 255;
    Mode.vSyncFreq.Numerator                   = VSync;
    Mode.vSyncFreq.Denominator                 = 1;
    Mode.hSyncFreq.Numerator                   = VSync * Height;
    Mode.hSyncFreq.Denominator                 = 1;
    Mode.scanLineOrdering                      = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    Mode.pixelRate                             = ((UINT64)VSync) * ((UINT64)Width) * ((UINT64)Height);
}

static IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width, DWORD Height, DWORD VSync)
{
    IDDCX_TARGET_MODE Mode = {};

    Mode.Size = sizeof(Mode);

    FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync, false);

    return Mode;
}

ULONG IddReadRegistryDword(WDFDEVICE Device, PCUNICODE_STRING ValueName)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG RegistryValue = 0, Length, ValueType;
    WDFKEY Regkey;

    // Read and update the registry values to select the type of monitor and preferred render adapter.
    Status = WdfDeviceOpenRegistryKey(Device, PLUGPLAY_REGKEY_DEVICE, PLUGPLAY_REGKEY_DEVICE, WDF_NO_OBJECT_ATTRIBUTES, &Regkey);
    if (!NT_SUCCESS(Status))
    {
        return RegistryValue;
    }

    Status = WdfRegistryQueryValue(
        Regkey,
        ValueName,
        sizeof(ULONG),
        &RegistryValue,
        &Length,
        &ValueType
    );
    if (!NT_SUCCESS(Status))
    {
        WdfRegistryClose(Regkey);
        return RegistryValue;
    }

    WdfRegistryClose(Regkey);
    return RegistryValue;
}

#pragma endregion

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_IDD_CX_DEVICE_IO_CONTROL                     IddSampleAdapterIoDeviceControl;
EVT_WDF_DRIVER_DEVICE_ADD                        IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY                          IddSampleDeviceD0Entry;
EVT_IDD_CX_ADAPTER_INIT_FINISHED                 IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES                  IddSampleAdapterCommitModes;
EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION             IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES            IddSampleMonitorQueryModes;
EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN              IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN            IddSampleMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

struct IndirectMonitorContextWrapper
{
    IndirectMonitorContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);
WDF_DECLARE_CONTEXT_TYPE(IndirectMonitorContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

#define DBG_PRINTF(...) {char msg[512]; sprintf_s(msg, 512, __VA_ARGS__);  OutputDebugStringA(msg);}

#define CURSOR_SOFTWARE 0
#define CURSOR_HARDWARE 1

#define REMOTE_SESSION 0x01000000

ULONG GetMonitorNumber()
{
    if (!MonitorNumberRegistryValue || MonitorNumberRegistryValue > 2)
    {
        return 1; // if the value read from registry is not proper, return 1.
    }

    return MonitorNumberRegistryValue;
}

ULONG GetMonitorCursor()
{
    if (MonitorCursorRegistryValue == CURSOR_HARDWARE)
    {
        return CURSOR_HARDWARE;
    }
    return CURSOR_SOFTWARE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WDF_DRIVER_CONFIG_INIT(&Config, IddSampleDeviceAdd);

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
    DECLARE_CONST_UNICODE_STRING(MonitorNumber, L"IddMonitorNumber");
    DECLARE_CONST_UNICODE_STRING(MonitorCursor, L"IddCursorControl");
    UNREFERENCED_PARAMETER(Driver);

    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;

    IddConfig.EvtIddCxAdapterInitFinished               = IddSampleAdapterInitFinished;
    IddConfig.EvtIddCxDeviceIoControl                   = IddSampleAdapterIoDeviceControl;
    IddConfig.EvtIddCxParseMonitorDescription           = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes           = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes                = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain            = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain          = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    MonitorNumberRegistryValue = IddReadRegistryDword(Device, &MonitorNumber);
    MonitorCursorRegistryValue = IddReadRegistryDword(Device, &MonitorCursor);

    // Create device interface for this device.
    Status = WdfDeviceCreateDeviceInterface(
        Device,
        &GUID_DEVINTERFACE_IDD_DEVICE,
        NULL // No Reference String. If you provide one it will appended to the
    );
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = IddCxDeviceInitialize(Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{
    AdapterLuid = LUID{};
}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent)
    : m_hSwapChain(hSwapChain), m_Device(Device), m_hAvailableBufferEvent(NewFrameEvent)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    // Get the DXGI device interface
    ComPtr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->Device.As(&DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if (FAILED(hr))
    {
        return;
    }

    // Acquire and release buffers in a loop
    for (;;)
    {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE WaitHandles[] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            // We have new frame to process, the surface has a reference on it that the driver has to release
            AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            // ==============================

            // We have finished processing this frame hence we release the reference on it.
            // If the driver forgets to release the reference to the surface, it will be leaked which results in the
            // surfaces being left around after swapchain is destroyed.
            // NOTE: Although in this sample we release reference to the surface here; the driver still
            // owns the Buffer.MetaData.pSurface surface until IddCxSwapChainReleaseAndAcquireBuffer returns
            // S_OK and gives us a new frame, a driver may want to use the surface in future to re-encode the desktop
            // for better quality if there is no new frame for a while
            AcquiredBuffer.Reset();

            // Indicate to OS that we have finished inital processing of the frame, it is a hint that
            // OS could start preparing another frame
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
    m_Adapter = {};
}

IndirectDeviceContext::~IndirectDeviceContext()
{
}

void IndirectDeviceContext::InitAdapter()
{
    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================

    IDDCX_ADAPTER_CAPS AdapterCaps = {0};

    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported                 = IDD_SAMPLE_MONITOR_COUNT;
    AdapterCaps.EndPointDiagnostics.Size             = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport     = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName     = L"Intel IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Intel IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName        = L"Intel IddSample Model";
    // AdapterCaps.Flags = IDDCX_ADAPTER_FLAGS_REMOTE_SESSION_DRIVER;

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version                   = {};
    Version.Size                                     = sizeof(Version);
    Version.MajorVer                                 = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice             = m_WdfDevice;
    AdapterInit.pCaps                 = &AdapterCaps;
    AdapterInit.ObjectAttributes      = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {
        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

void IndirectDeviceContext::FinishInit(UINT ConnectorIndex)
{
    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDIDs
    // provided here are purely for demonstration.
    // Monitor manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS
    // to optimize settings like viewing distance and scale factor. Manufacturers should also use a unique serial
    // number every single device to ensure the OS can tell the monitors apart.
    // ==============================

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectMonitorContextWrapper);

    // In the sample driver, we report a monitor right away but a real driver would do this when a monitor connection event occurs
    IDDCX_MONITOR_INFO MonitorInfo      = {};
    MonitorInfo.Size                    = sizeof(MonitorInfo);
    // Reporting as INDIRECT_WIRED for detecting IDD displays in QueryDisplayConfig Win API.
    MonitorInfo.MonitorType             = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_WIRED;
    MonitorInfo.ConnectorIndex          = ConnectorIndex;
    MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
    MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    // ==============================

    // Create a container ID
    HRESULT hr = CoCreateGuid(&MonitorInfo.MonitorContainerId);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes       = &Attr;
    MonitorCreate.pMonitorInfo           = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    IDARG_OUT_MONITORCREATE MonitorCreateOut;
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if (NT_SUCCESS(Status))
    {
        // Create a new monitor context object and attach it to the Idd monitor object
        auto* pMonitorContextWrapper                = WdfObjectGet_IndirectMonitorContextWrapper(MonitorCreateOut.MonitorObject);
        pMonitorContextWrapper->pContext            = new IndirectMonitorContext(MonitorCreateOut.MonitorObject);
        pMonitorContextWrapper->pContext->m_Adapter = m_Adapter;

        // Tell the OS that the monitor has been plugged in
        IDARG_OUT_MONITORARRIVAL ArrivalOut;
        Status = IddCxMonitorArrival(MonitorCreateOut.MonitorObject, &ArrivalOut);
        if (!NT_SUCCESS(Status))
        {
            return;
        }
    }
}

NTSTATUS IndirectDeviceContext::UpdateLUID(PIDD_UPDATE_LUID pUpdateLUID)
{
    NTSTATUS Status = STATUS_SUCCESS;

    //TraceEvents(TRACE_LEVEL_ERROR,TRACE_DEVICE,"%!FUNC! LUID ( %ud.%ud)", pUpdateLUID->luid.LowPart, pUpdateLUID->luid.HighPart);
    if (AdapterLUIDLowPart != pUpdateLUID->luid.LowPart)
    {
        IDARG_IN_ADAPTERSETRENDERADAPTER PreferredAdapter;
        AdapterLUIDLowPart                               = pUpdateLUID->luid.LowPart;
        PreferredAdapter.PreferredRenderAdapter.HighPart = pUpdateLUID->luid.HighPart;
        PreferredAdapter.PreferredRenderAdapter.LowPart  = pUpdateLUID->luid.LowPart;

        IddCxAdapterSetRenderAdapter(m_Adapter, &PreferredAdapter);
        return STATUS_GRAPHICS_INDIRECT_DISPLAY_ABANDON_SWAPCHAIN;
    }

    return Status;
}

NTSTATUS IndirectDeviceContext::CheckAndSetRenderAdapter(LUID RenderAdapter)
{
    if (AdapterLUIDLowPart != RenderAdapter.LowPart)
    {
        IDARG_IN_ADAPTERSETRENDERADAPTER PreferredAdapter;
        PreferredAdapter.PreferredRenderAdapter.LowPart = AdapterLUIDLowPart;
        IddCxAdapterSetRenderAdapter(m_Adapter, &PreferredAdapter);
        return STATUS_GRAPHICS_INDIRECT_DISPLAY_ABANDON_SWAPCHAIN;
    }
    return STATUS_SUCCESS;
}

IndirectMonitorContext::IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor) :
    m_Monitor(Monitor)
{
    m_Adapter = {};
}

IndirectMonitorContext::~IndirectMonitorContext()
{
    m_ProcessingThread.reset();
}

NTSTATUS IndirectMonitorContext::AssignSwapChain(IDDCX_MONITOR MonitorObject,IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    m_ProcessingThread.reset();
    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    if (FAILED(Device->Init()))
    {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
        IDARG_IN_ADAPTERSETRENDERADAPTER PreferredAdapter;
        if (AdapterLUIDLowPart != 0 && AdapterLUIDLowPart != RenderAdapter.LowPart)
        {
            PreferredAdapter.PreferredRenderAdapter.LowPart = AdapterLUIDLowPart;
            IddCxAdapterSetRenderAdapter(pMonitorContextWrapper->pContext->m_Adapter, &PreferredAdapter);
            return STATUS_GRAPHICS_INDIRECT_DISPLAY_ABANDON_SWAPCHAIN;
        }

    }
    else
    {
        // Create a new swap-chain processing thread
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent));

        if (GetMonitorCursor() == CURSOR_HARDWARE)
        {
            HANDLE hCursorData = CreateEventA(NULL, FALSE, FALSE, NULL);
            if (hCursorData == NULL)
            {
                DBG_PRINTF("IDD : CreateEventA return NULL\n");
                return STATUS_UNSUCCESSFUL;
            }

            // Setup Hardware cursor
            IDARG_IN_SETUP_HWCURSOR IdArgHwCursor;
            ZeroMemory(&IdArgHwCursor, sizeof(IdArgHwCursor));
            IdArgHwCursor.CursorInfo.Size                  = sizeof(IDDCX_CURSOR_CAPS);
            IdArgHwCursor.CursorInfo.ColorXorCursorSupport = IDDCX_XOR_CURSOR_SUPPORT_FULL;
            IdArgHwCursor.CursorInfo.AlphaCursorSupport    = TRUE;
            IdArgHwCursor.CursorInfo.MaxX                  = 256;
            IdArgHwCursor.CursorInfo.MaxY                  = 256;
            IdArgHwCursor.hNewCursorDataAvailable          = hCursorData;

            NTSTATUS Status = IddCxMonitorSetupHardwareCursor(MonitorObject, &IdArgHwCursor);
            DBG_PRINTF("IDD : IddCxMonitorSetupHardwareCursor Status (0x%x)\n", Status);
            if (!NT_SUCCESS(Status))
            {
                return Status;
            }
        }
        return STATUS_SUCCESS;
    }
    return STATUS_SUCCESS;
}

void IndirectMonitorContext::UnassignSwapChain()
{
    // Stop processing the last swap-chain
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.
    auto* pDeviceContextWrapper = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
    if (NT_SUCCESS(pInArgs->AdapterInitStatus) && pDeviceContextWrapper)
    {
        for (DWORD i = 0; i < GetMonitorNumber(); i++)
        {
            pDeviceContextWrapper->pContext->FinishInit(i);
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
    // This function is called to parse monitor EDID. We configured EDID-less
    // monitor so we should not get it called. However, Windows does seem to
    // expect that we will provide valid function callback to parse EDIDs.
    (void)pInArgs;
    (void)pOutArgs;
    return STATUS_INVALID_PARAMETER;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    // This function is called to generate monitor modes for monitors without EDID
    // which is exactly our case. In case we will need to support EDIDs for any
    // reason, here we can report additional modes not listed in EDID.
    UNREFERENCED_PARAMETER(MonitorObject);

    if (pInArgs->DefaultMonitorModeBufferInputCount == 0)
    {
        pOutArgs->DefaultMonitorModeBufferOutputCount = ARRAYSIZE(g_modes);
        return STATUS_SUCCESS;
    }
    if (pInArgs->DefaultMonitorModeBufferInputCount < ARRAYSIZE(g_modes))
    {
        return STATUS_INVALID_PARAMETER;
    }

    UINT def_idx = 0;
    for (DWORD idx = 0; idx < ARRAYSIZE(g_modes); ++idx)
    {
        pInArgs->pDefaultMonitorModes[idx] = {};
        pInArgs->pDefaultMonitorModes[idx].Size = sizeof(pInArgs->pDefaultMonitorModes[idx]);
        pInArgs->pDefaultMonitorModes[idx].Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER;

        FillSignalInfo(
            pInArgs->pDefaultMonitorModes[idx].MonitorVideoSignalInfo,
            g_modes[idx].width,
            g_modes[idx].height,
            g_modes[idx].vsync, true);

        if (g_modes[idx].width == g_default_mode.width &&
            g_modes[idx].height == g_default_mode.height &&
            g_modes[idx].vsync == g_default_mode.vsync)
        {
            def_idx = idx;
        }
    }

    pOutArgs->PreferredMonitorModeIdx = def_idx;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes;

    for (DWORD idx = 0; idx < ARRAYSIZE(g_modes); ++idx)
    {
        IDDCX_TARGET_MODE mode{};

        mode.Size = sizeof(mode);
        FillSignalInfo(
            mode.TargetVideoSignalInfo.targetVideoSignalInfo,
            g_modes[idx].width,
            g_modes[idx].height,
            g_modes[idx].vsync,
            false);
        TargetModes.push_back(mode);
    }

    pOutArgs->TargetModeBufferOutputCount = (UINT)TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    return pMonitorContextWrapper->pContext->AssignSwapChain(MonitorObject, pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    pMonitorContextWrapper->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
IddSampleAdapterIoDeviceControl(WDFDEVICE Device, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(Device);

    NTSTATUS Status = STATUS_SUCCESS;
    PVOID  Buffer;
    size_t BufSize;
    int bytesRead  = 0;
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    switch (IoControlCode)
    {
    case IOCTL_IDD_UPDATE_LUID:
        PIDD_UPDATE_LUID pUpdateLUID;
        Status = WdfRequestRetrieveInputBuffer(Request, sizeof(IDD_UPDATE_LUID), &Buffer, &BufSize);
        if (!NT_SUCCESS(Status))
        {
            break;
        }
        pUpdateLUID = (PIDD_UPDATE_LUID)Buffer;
        pContext->pContext->UpdateLUID(pUpdateLUID);
        Status = STATUS_SUCCESS;
        break;
     default:
        Status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    WdfRequestCompleteWithInformation(Request, Status, bytesRead);
}

#pragma endregion
