// Copyright (C) Microsoft Corporation
// Copyright (C) 2022-2023 Intel Corporation
//
// This file contains modifications made to the original Microsoft sample code from
// https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay/IddSampleApp
//
// List of modifications:
// 1. Add multi-adapter support

#include "PairIdd.h"
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <windows.h>
#include <swdevice.h>
#include <conio.h>
#include <wrl.h>
#include <setupapi.h>
#include <strsafe.h>
#include <dxgi1_6.h>
#include <atlcomcli.h>
#include "idd_io.h"

using namespace std;

const GUID GUID_DEVINTERFACE_IDD_DEVICE = {
    0x881EF630,
    0x82B2,
    0x81d2,
    {
        0x88,
        0x82,
        0x80,
        0x80,
        0x8E,
        0x8F,
        0x82,
        0x82
    } 
 };

#define INTERFACE_DETAIL_SIZE        1024
#define MSFT_BASIC_DISPLAY_ADAPTER 0x1414

static std::vector<LUID> IddLUIDs;
static std::vector<LUID> GpuLUIDs;

/**
 * DXGI Adapter Information
 */
struct DxgiAdapterInfo
{
    uint32_t adapterIndex;    //!< adapter index
    DXGI_ADAPTER_DESC1 desc;  //!< DXGI adapter description
};

std::vector<tstring> GetDevicePath()
{
    std::vector<tstring> pszDevicePath;
    HDEVINFO                         hDevInfoSet;
    SP_DEVICE_INTERFACE_DATA         ifdata;
    PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail;
    UINT32 ifIndex = 0;
    BOOL   bStatus = TRUE;
    GUID  virtDispGuid = GUID_DEVINTERFACE_IDD_DEVICE;
    IDD_STATUS Status =IDD_STATUS_SUCCESS;

    // Get a Device Info handle that relates to provided class GUID
    hDevInfoSet = SetupDiGetClassDevs(
        &virtDispGuid,                        // Class GUID
        NULL,                                 // No Enumerator
        NULL,                                 // No Parent Window
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE // Enumerate all present interfaces
    );

    if (hDevInfoSet == INVALID_HANDLE_VALUE) {
        printf("IDD: not present any interface relates to Intel IDD\n");
        return pszDevicePath;
    }

    pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)GlobalAlloc(LMEM_ZEROINIT, INTERFACE_DETAIL_SIZE);
    pDetail->cbSize = (DWORD)sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // Enumerates all the interfaces
    while (bStatus) {
        ifdata.cbSize = sizeof(ifdata);
        //Enumerates interface specified by the interface order, start from 0
        bStatus = SetupDiEnumDeviceInterfaces(
            hDevInfoSet,
            NULL,
            &virtDispGuid,
            (ULONG)ifIndex,
            &ifdata
        );

        if (bStatus) {
            bStatus = SetupDiGetInterfaceDeviceDetail(
                hDevInfoSet,
                &ifdata,
                pDetail,
                INTERFACE_DETAIL_SIZE,
                NULL,
                NULL
            );

            if (bStatus) {
                tstring tempString = pDetail->DevicePath;
                pszDevicePath.push_back(std::move(tempString));
                ifIndex++;
            }
        }
    }

    GlobalFree(pDetail);
    SetupDiDestroyDeviceInfoList(hDevInfoSet);

    return pszDevicePath;
}

/**
 * Open a handle to the Idd Driver.
 *
 */
IDD_STATUS OpenVirtualDisplay(IN tstring pszDevicePath, OUT HANDLE* handle)
{
    // wait till new interface is ready
    IDD_STATUS Status = IDD_STATUS_SUCCESS;
    if (NULL == handle || pszDevicePath.size() == 0) {
        return IDD_INVALID_HANDLE;
    }

    do {
        *handle = CreateFile(
            pszDevicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,                               // no SECURITY_ATTRIBUTES structure
            OPEN_EXISTING,
            0,                                  // No special attributes
            NULL
        );

        if (*handle == INVALID_HANDLE_VALUE || *handle == NULL) {
            Status = IDD_STATUS_ACCESS_DENIED;
            printf("OpenVirtualDisplay: Failed to createFile error=0x%x\n", GetLastError());
        }
    } while (0);

    return Status;
}

/**
 * Find out if an Adapter is Idd adapter.
 * Using Query Display Config, all of the adapters including Idd should have been enumerated.
 * If the adapter LUID matches and Idd LUID, mark it as idd adapter.
 */
bool IsIddAdapter(LUID luid)
{
    if (!IddLUIDs.empty()) {
        for (auto& IddLUID : IddLUIDs) {
            if (luid.HighPart == IddLUID.HighPart && luid.LowPart == IddLUID.LowPart) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Find out if a given path has Idd monitor.
 * Using Query Display Config, all of the adapters including Idd should have been enumerated.
 * If the Video Output Technology type matches, Indirect Wired/virtual or other mark it as an idd path/adapter.
 */
bool IsIddPath(DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY VotType)
{
    if  (//(VotType == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER) ||
        (VotType == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_WIRED) ||
        (VotType == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_VIRTUAL))
    {
        return true;
    }

    return false;
}

/**
 * Get Adapter LUIDs.
 * Using Query Display Config, find out all the idd adapters and store them in IddLUIDs.
 * Using Enumerate adapters find out all the adapters in the system (both idd and non idd)
 * Filter out the Idd adapters using the IddLUIDs list. All the other adapters must be physical adapters
 * This can be further enhanced to have vendor id/device id checks for further restrictions. .
 */
void GetAdapterLUIDs()
{
    LUID IddLUID = { 0 };
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    // UINT32 flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
    UINT32 flags = QDC_ALL_PATHS | QDC_VIRTUAL_MODE_AWARE;
    LONG result = ERROR_SUCCESS;

    tcout << FormatOutput(TEXT("Querying Display Adapter LUIDs")) << std::endl;

    do {
        // Determine how many path and mode structures to allocate
        UINT32 pathCount, modeCount;
        result = GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount);

        if (result != ERROR_SUCCESS) {
            tcout << FormatOutputWithOffset(1, TEXT("GetAdapterLUIDs: Query Display Config Failure")) << std::endl;
            return;
        }

        // Allocate the path and mode arrays
        paths.resize(pathCount);
        modes.resize(modeCount);

        // Get all active paths and their modes
        result = QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);

        // The function may have returned fewer paths/modes than estimated
        paths.resize(pathCount);
        modes.resize(modeCount);

        // It's possible that between the call to GetDisplayConfigBufferSizes and QueryDisplayConfig
        // that the display state changed, so loop on the case of ERROR_INSUFFICIENT_BUFFER.
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    // For each active path
    for (auto& path : paths) {
        // If its an idd display add to the IddLUIDs.
        if (IsIddPath(path.targetInfo.outputTechnology)) {
            IddLUIDs.push_back(path.targetInfo.adapterId);// Store the Idd LUID
        }
    }

    std::vector<DxgiAdapterInfo> gpuAdapterDescs;

    CComPtr<IDXGIFactory6> dxgiFactory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr) || !dxgiFactory) {
        return;
    }

    // Enumerating adapters information
    CComPtr<IDXGIAdapter1> adapter;

    for (UINT adapterIdx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(adapterIdx, &adapter); ++adapterIdx) {
        DxgiAdapterInfo adapterInfo{};
        adapterInfo.adapterIndex = adapterIdx;
        adapter->GetDesc1(&adapterInfo.desc);

        // Ignore software adapter/basic render adapter.
        if ((adapterInfo.desc.VendorId != MSFT_BASIC_DISPLAY_ADAPTER)) {
            gpuAdapterDescs.push_back(std::move(adapterInfo));
        }
        adapter.Release();
    }

    if (!gpuAdapterDescs.empty()) {
        for (auto& adapterInfo : gpuAdapterDescs) {
            if (IsIddAdapter(adapterInfo.desc.AdapterLuid)) {
                tcout << FormatOutputWithOffset(1, TEXT("Idd Adapter LUID: High Part = 0x%x, Low Part = 0x%x"), adapterInfo.desc.AdapterLuid.HighPart, adapterInfo.desc.AdapterLuid.LowPart) << std::endl;
            } else {
                // Store the non-idd LUIDs in a list.
                GpuLUIDs.push_back(adapterInfo.desc.AdapterLuid);
                tcout << FormatOutputWithOffset(1, TEXT("Physical Adapter LUID: High Part = 0x%x, Low Part = 0x%x"), adapterInfo.desc.AdapterLuid.HighPart, adapterInfo.desc.AdapterLuid.LowPart) << std::endl;
            }
        }
    }
}

/**
 * Call the Idd IOCTL to update LUID.
 */
IDD_STATUS  UpdateAdapterLUID(IN HANDLE hDevice, IN LUID luid)
{
    IDD_STATUS Status = IDD_STATUS_SUCCESS;
    IDD_UPDATE_LUID UpdateLUID = { 0 };
    DWORD bytesReturn = 0;

    tcout << FormatOutputWithOffset(1, TEXT("IDD LUID Update: Pairing with LUID high part = 0x%x, LUID low part = 0x%x"), luid.HighPart, luid.LowPart) << std::endl;

    if (hDevice == NULL) {
        tcout << FormatOutputWithOffset(2, TEXT("IDD LUID Update: Pairing failed. Null Handle passed")) << std::endl;
        return IDD_STATUS_ACCESS_DENIED;
    }

    UpdateLUID.luid = luid;
    if (TRUE == DeviceIoControl(
        hDevice,
        IOCTL_IDD_UPDATE_LUID,
        &UpdateLUID,                 // Ptr to InBuffer
        sizeof(UpdateLUID),          // Length of InBuffer
        NULL,                        // Ptr to OutBuffer
        0,                           // Length of OutBuffer
        &bytesReturn,                // BytesReturned
        0))                          // Ptr to Overlapped structure
    {
        Status = IDD_STATUS_SUCCESS;
        tcout << FormatOutputWithOffset(2, TEXT("IDD LUID Update: Pairing succeeded")) << std::endl;
    } else {
        Status = IDD_INVALID_PARAM;
        DWORD error = GetLastError();
        tcout << FormatOutputWithOffset(2, TEXT("IDD LUID Update: Pairing failed with error %x"), error) << std::endl;
    }

    return Status;
}

/**
 * Enumerate all the adapters in the systems.
 * Using Query Display Config, find out all the idd adapters and store them in IddLUIDs.
 * Using Enumerate adapters find out all the adapters in the system (both idd and non idd)
 * Filter out the Idd adapters using the IddLUIDs list. All the other adapters must be physical adapters
 * This can be further enhanced to have vendor id/device id checks for further restrictions. .
 */
void PairIddLUIDsToGpuLUIDs(void)
{
    IDD_STATUS Status = IDD_STATUS_SUCCESS;
    GetAdapterLUIDs();

    std::vector<tstring> path = GetDevicePath();
    tcout << FormatOutput(TEXT("Found %llu IddAdapters"), path.size()) << std::endl;
    for (int i = 0; i < path.size(); i++) {
        tcout << FormatOutput(TEXT("Opening IDD device: %s"), path[i].c_str()) << std::endl;
        HANDLE handle;
        Status = OpenVirtualDisplay(path[i], &handle);
        if (Status == IDD_STATUS_SUCCESS) {
            LUID luid;
            auto noOfPhysicalAdapter = GpuLUIDs.size();
            luid = GpuLUIDs.at(i % noOfPhysicalAdapter);
            UpdateAdapterLUID(handle, luid);
            CloseHandle(handle);
        }
    }
}
