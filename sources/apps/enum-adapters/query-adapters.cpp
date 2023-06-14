// Copyright (C) 2022-2023 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file   query-adapters.cpp
 *
 * @brief  Functions for query D3D adapter device information using kernel mode thunk interfaces.
 *
 */

#include "query-adapters.h"
#include <sstream>
#include <algorithm>
#include <winternl.h>
#include <d3dkmthk.h>

#if !defined(STATUS_SUCCESS)
    #define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)    // ntsubauth
#endif // !defined(STATUS_SUCCESS)

 /**
  * @brief GUID to wstring helper function
  *
  * @param guid  GUID to convert
  *
  * @return none
  *
  */
std::wstring GUID_to_wstring(const GUID& guid)
{
    LPOLESTR olestr;
    if (FAILED(StringFromCLSID(guid, &olestr))) {
        return L"Unknown";
    }

    std::wstring w_guid(olestr, olestr + wcslen(olestr));
    CoTaskMemFree(olestr);

    return w_guid;
}

/**
 * @brief Show adapter's device information
 *
 * @param info      Adapter's device information
 * @param details   Show information in details
 *
 * @return none
 *
 */
void show_adapter_device_info(const AdapterDeviceInfo& info, bool details /*= false*/)
{
    std::wcout << L"            Description : " << info.description << std::endl;
    std::wcout << L"                   LUID : " << info.luid.HighPart << L":" << info.luid.LowPart
        << L" [0x" << std::hex << info.luid.HighPart << L":0x" << info.luid.LowPart << L"]" << std::dec << std::endl;
    std::wcout << L"              Vendor ID : 0x" << std::hex << info.vendorID << std::dec << std::endl;
    std::wcout << L"              Device ID : 0x" << std::hex << info.deviceID << std::dec << std::endl;
    std::wcout << L"           Subvendor ID : 0x" << std::hex << info.subVendorID << std::dec << std::endl;
    std::wcout << L"           Subsystem ID : 0x" << std::hex << info.subSystemID << std::dec << std::endl;
    std::wcout << L"            Revision ID : 0x" << std::hex << info.revisionID << std::dec << std::endl;
    std::wcout << L"    Enum. Adapter Index : " << info.adapterIndex << std::endl;

    if (details) {
        std::wcout << L"         Adapter Handle : 0x" << std::hex << info.handler << std::dec << std::endl;
        std::wcout << L"      Number of Sources : " << info.numSources << std::endl;

        std::wcout << std::endl;
        std::wcout << L"       GPU BIOS Version : " << info.gpuBiosVersion << std::endl;
        std::wcout << L"       GPU Architecture : " << info.gpuArchitecture << std::endl;

        std::wcout << L"                   Name : " << info.name << std::endl;
        std::wcout << L"                   BIOS : " << info.bios << std::endl;
        std::wcout << L"               Dac Type : " << info.dacType << std::endl;
        std::wcout << L"              Chip Type : " << info.chipType << std::endl;

        std::wcout << L"    Phys. Adapter Count : " << info.physicalAdapterCount << std::endl;
        std::wcout << L"    Phys. Adapter Index : " << info.physicalAdapterIndex << std::endl;
        std::wcout << L"               Bus Type : " << info.busType << std::endl;

        std::wcout << L"    Bus Device Function : " << info.bus << L" , " << info.device << L" , " << info.function
            << L" [0x" << std::hex << info.bus << L" , 0x" << info.device << L" , 0x" << info.function << L"]" << std::dec << std::endl;

        std::wcout << L"                   GUID : " << GUID_to_wstring(info.guid) << std::endl;

        std::wcout << L"           Video Memory : " << info.vram << std::endl;
        std::wcout << L"          System Memory : " << info.sysRam << std::endl;
        std::wcout << L"     Shared Sys. Memory : " << info.sharedRam << std::endl;

        std::wcout << std::endl;
        std::wcout << L"           RenderSupported : " << info.renderSupported << std::endl;
        std::wcout << L"          DisplaySupported : " << info.displaySupported << std::endl;
        std::wcout << L"            SoftwareDevice : " << info.softwareDevice << std::endl;
        std::wcout << L"                PostDevice : " << info.postDevice << std::endl;
        std::wcout << L"            HybridDiscrete : " << info.hybridDiscrete << std::endl;
        std::wcout << L"          HybridIntegrated : " << info.hybridIntegrated << std::endl;
        std::wcout << L"     IndirectDisplayDevice : " << info.indirectDisplayDevice << std::endl;
        std::wcout << L"           Paravirtualized : " << info.paravirtualized << std::endl;
        std::wcout << L"              ACGSupported : " << info.acgSupported << std::endl;
        std::wcout << L"SupportSetTimingsFromVidPn : " << info.supportSetTimingsFromVidPn << std::endl;
        std::wcout << L"                Detachable : " << info.detachable << std::endl;
        std::wcout << L"               ComputeOnly : " << info.computeOnly << std::endl;
        std::wcout << L"                 Prototype : " << info.prototype << std::endl;
        std::wcout << L"    RuntimePowerManagement : " << info.runtimePowerManagement << std::endl;
    }
    std::wcout << std::endl;
}

/**
 * @brief Check valid LUID from string
 *
 * @param luidstr String contained LUID
 * @param luid    Return LUID if valid
 *
 * @return true if success, false otherwise
 *
 */
bool check_luid(std::string luidstr, LUID& luid)
{
    std::vector<unsigned long> aluid;
    std::replace(luidstr.begin(), luidstr.end(), ':', ' ');
    std::stringstream ss(luidstr);
    std::string f;
    while (ss >> f) {
        try {
            aluid.push_back(std::stoul(f, nullptr, 0));
        }
        catch (std::exception& ex) {
            std::cerr << ex.what() << ": " << f << std::endl;
            return false;
        }
    }

    if (aluid.size() < 2) {
        std::cerr << "LUID parsing failed. Incorrect LUID argument: \"" << luidstr << "\". Use '-h' for list correct format." << std::endl;
        return false;
    }

    luid.HighPart = aluid[0];
    luid.LowPart = aluid[1];

    return true;
}

/**
 * @brief Query D3D/GPU adapters list
 *
 * @param physicalDevices   List of physical adapter devices list
 * @param indirectDevices   List of indirect adapter devices list
 * @param softwareDevices   List of software adapter devices list
 * @param debug             Show debug messages for debugging purpose
 *
 * @return true if success, false otherwise
 *
 */
bool query_adapters_list(AdapterDeviceInfoList& physicalDevices, AdapterDeviceInfoList& indirectDevices, AdapterDeviceInfoList& softwareDevices, bool debug /*= false*/)
{
    D3DKMT_ENUMADAPTERS enumAdapters{};
    NTSTATUS status = D3DKMTEnumAdapters(&enumAdapters);
    if (STATUS_SUCCESS != status) {
        return false;
    }

    for (ULONG i = 0; i < enumAdapters.NumAdapters; ++i) {
        D3DKMT_OPENADAPTERFROMLUID adapterFromLuid{};
        adapterFromLuid.hAdapter = enumAdapters.Adapters[i].hAdapter;
        adapterFromLuid.AdapterLuid = enumAdapters.Adapters[i].AdapterLuid;
        if (D3DKMTOpenAdapterFromLuid(&adapterFromLuid) == STATUS_SUCCESS) {
            D3DKMT_PHYSICAL_ADAPTER_COUNT adapterCount{};
            D3DKMT_QUERYADAPTERINFO adapterInfo{};
            D3DKMT_ADAPTERREGISTRYINFO adapterRegInfo{};
            D3DKMT_ADAPTERADDRESS adapterAddress{};
            D3DKMT_SEGMENTSIZEINFO segmentSizeInfo{};
            D3DKMT_ADAPTERTYPE adapterType{};
            D3DKMT_QUERY_DEVICE_IDS deviceIds{};
            D3DKMT_DRIVER_DESCRIPTION driverDescription{};
            D3DKMT_UMD_DRIVER_VERSION umdDriverVersion{};
            D3DKMT_KMD_DRIVER_VERSION kmdDriverVersion{};
            D3DKMT_GPUVERSION gpuVersion{};
            GUID deviceGuid{};

            if (debug)
                std::cout << "Enumerated Adapter Index: " << i << std::endl;

            // Query: Registry information about the graphics adapter
            adapterInfo.Type = KMTQAITYPE_ADAPTERREGISTRYINFO;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &adapterRegInfo;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_ADAPTERREGISTRYINFO);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_ADAPTERREGISTRYINFO      0x" << std::hex << (status) << std::dec << std::endl;

            // Query: Describes the physical location of the graphics adapter.
            adapterInfo.Type = KMTQAITYPE_ADAPTERADDRESS;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &adapterAddress;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_ADAPTERADDRESS);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_ADAPTERADDRESS           0x" << std::hex << (status) << std::dec << std::endl;

            // Query: The GUID for the adapter.
            adapterInfo.Type = KMTQAITYPE_ADAPTERGUID;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &deviceGuid;
            adapterInfo.PrivateDriverDataSize = sizeof(GUID);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  Device Adapter GUID             0x" << std::hex << (status) << std::dec << std::endl;

            // Query: Describes the size, in bytes, of memory and aperture segments.
            adapterInfo.Type = KMTQAITYPE_GETSEGMENTSIZE;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &segmentSizeInfo;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_SEGMENTSIZEINFO);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_SEGMENTSIZEINFO          0x" << std::hex << (status) << std::dec << std::endl;

            // Query: Specifies the type of display device that the graphics adapter supports.
            adapterInfo.Type = KMTQAITYPE_ADAPTERTYPE;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &adapterType;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_ADAPTERTYPE);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_ADAPTERTYPE              0x" << std::hex << (status) << std::dec << std::endl;

            // Query: Used to get the physical adapter count.
            adapterInfo.Type = KMTQAITYPE_PHYSICALADAPTERCOUNT;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &adapterCount;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_PHYSICAL_ADAPTER_COUNT);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_PHYSICAL_ADAPTER_COUNT   0x" << std::hex << (status) << std::dec << " count = " << adapterCount.Count << std::endl;

            for (ULONG j = 0; j < enumAdapters.NumAdapters; ++j) {
                deviceIds.PhysicalAdapterIndex = j;
                // Query: Used to query for device IDs.
                adapterInfo.Type = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
                adapterInfo.hAdapter = adapterFromLuid.hAdapter;
                adapterInfo.pPrivateDriverData = &deviceIds;
                adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_QUERY_DEVICE_IDS);
                status = D3DKMTQueryAdapterInfo(&adapterInfo);

                if (debug)
                    std::cout << "  D3DKMT_QUERY_DEVICE_IDS         0x" << (status) << " index: " << j << std::endl;
                if (STATUS_SUCCESS == status)
                    break;
            }
            if (debug)
                std::cout << std::endl;

            if (status != STATUS_SUCCESS) {
                deviceIds.DeviceIds.DeviceID = 0;
                deviceIds.DeviceIds.VendorID = 0;
                status = 0;
            }

            // Query: Used to collect the bios version and GPU architecture name once during GPU initialization.
            adapterInfo.Type = KMTQUITYPE_GPUVERSION;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &gpuVersion;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_GPUVERSION);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_GPUVERSION               0x" << std::hex << (status) << std::dec << ". GPU Phy. index = " << gpuVersion.PhysicalAdapterIndex << std::endl;

            // Query: Describes the kernel mode display driver.
            adapterInfo.Type = KMTQAITYPE_DRIVER_DESCRIPTION;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &driverDescription;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_DRIVER_DESCRIPTION);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_DRIVER_DESCRIPTION       0x" << std::hex << (status) << std::dec << std::endl;

            // Query: Indicates the version number of the user-mode driver.
            adapterInfo.Type = KMTQAITYPE_UMD_DRIVER_VERSION;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &umdDriverVersion;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_UMD_DRIVER_VERSION);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_UMD_DRIVER_VERSION       0x" << std::hex << (status) << " " << umdDriverVersion.DriverVersion.QuadPart << std::dec << std::endl;

            // Query: The kernel mode driver version.
            adapterInfo.Type = KMTQAITYPE_KMD_DRIVER_VERSION;
            adapterInfo.hAdapter = adapterFromLuid.hAdapter;
            adapterInfo.pPrivateDriverData = &kmdDriverVersion;
            adapterInfo.PrivateDriverDataSize = sizeof(D3DKMT_KMD_DRIVER_VERSION);
            status = D3DKMTQueryAdapterInfo(&adapterInfo);

            if (debug)
                std::cout << "  D3DKMT_KMD_DRIVER_VERSION       0x" << std::hex << (status) << " " << kmdDriverVersion.DriverVersion.QuadPart << std::dec << std::endl;

            AdapterDeviceInfo devInfo;

            devInfo.adapterIndex = i;

            devInfo.description = std::wstring(driverDescription.DriverDescription);

            devInfo.gpuBiosVersion = std::wstring(gpuVersion.BiosVersion);
            devInfo.gpuArchitecture = std::wstring(gpuVersion.GpuArchitecture);

            devInfo.name = std::wstring(adapterRegInfo.AdapterString);
            devInfo.bios = std::wstring(adapterRegInfo.BiosString);
            devInfo.dacType = std::wstring(adapterRegInfo.DacType);
            devInfo.chipType = std::wstring(adapterRegInfo.ChipType);

            devInfo.luid = enumAdapters.Adapters[i].AdapterLuid;
            devInfo.handler = enumAdapters.Adapters[i].hAdapter;
            devInfo.numSources = enumAdapters.Adapters[i].NumOfSources;

            devInfo.bus = adapterAddress.BusNumber;
            devInfo.device = adapterAddress.DeviceNumber;
            devInfo.function = adapterAddress.FunctionNumber;

            devInfo.guid = deviceGuid;

            devInfo.vram = segmentSizeInfo.DedicatedVideoMemorySize;
            devInfo.sysRam = segmentSizeInfo.DedicatedSystemMemorySize;
            devInfo.sharedRam = segmentSizeInfo.SharedSystemMemorySize;

            devInfo.physicalAdapterCount = adapterCount.Count;

            devInfo.physicalAdapterIndex = deviceIds.PhysicalAdapterIndex;
            devInfo.vendorID = deviceIds.DeviceIds.VendorID;
            devInfo.deviceID = deviceIds.DeviceIds.DeviceID;
            devInfo.busType = deviceIds.DeviceIds.BusType;
            devInfo.revisionID = deviceIds.DeviceIds.RevisionID;
            devInfo.subSystemID = deviceIds.DeviceIds.SubSystemID;
            devInfo.subVendorID = deviceIds.DeviceIds.SubVendorID;

            devInfo.renderSupported = adapterType.RenderSupported;
            devInfo.displaySupported = adapterType.DisplaySupported;
            devInfo.softwareDevice = adapterType.SoftwareDevice;
            devInfo.postDevice = adapterType.PostDevice;
            devInfo.hybridDiscrete = adapterType.HybridDiscrete;
            devInfo.hybridIntegrated = adapterType.HybridIntegrated;
            devInfo.indirectDisplayDevice = adapterType.IndirectDisplayDevice;
            devInfo.paravirtualized = adapterType.Paravirtualized;
            devInfo.acgSupported = adapterType.ACGSupported;
            devInfo.supportSetTimingsFromVidPn = adapterType.SupportSetTimingsFromVidPn;
            devInfo.detachable = adapterType.Detachable;
            devInfo.computeOnly = adapterType.ComputeOnly;
            devInfo.prototype = adapterType.Prototype;
            devInfo.runtimePowerManagement = adapterType.RuntimePowerManagement;

            if (!devInfo.softwareDevice) {
                if (!devInfo.indirectDisplayDevice) {
                    physicalDevices.push_back(devInfo);
                }
                else {
                    indirectDevices.push_back(devInfo);
                }
            }
            else {
                softwareDevices.push_back(devInfo);
            }
            {
                D3DKMT_CLOSEADAPTER closeAdapter;
                closeAdapter.hAdapter = adapterFromLuid.hAdapter;
                D3DKMTCloseAdapter(&closeAdapter);
            }
        }
    }

    return true;
}
