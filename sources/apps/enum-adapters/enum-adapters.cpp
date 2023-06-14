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

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <atlcomcli.h>
#include <dxgi1_6.h>
#include "cg-version.h"
#include "query-adapters.h"

/**
 * HRESULT code to string
 *
 * @param hr   HRESULT code
 *
 * @return HRESULT string
 */
inline std::string HrToString(HRESULT hr)
{
  std::stringstream ss;
  ss << "HRESULT of 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << hr;
  return ss.str();
}

/**
 * HRESULT exception class
 */
class HrException : public std::runtime_error
{
public:
  HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), hr_(hr) {}
  HRESULT Error() const { return hr_; }
private:
  HRESULT hr_;
};

/**
 * Throw if failed HRESULT code
 *
 * @param hr   HRESULT code
 */
inline void ThrowIfFailed(HRESULT hr)
{
  if (FAILED(hr)) {
    throw HrException(hr);
  }
}

/**
 * DXGI Output Information
 */
struct DxgiOutputInfo
{
    uint32_t outputIndex;       //!< output index
    DXGI_OUTPUT_DESC desc;      //!< DXGI output description
    MONITORINFOEXW monitorInfo; //!< Monitor information
};
typedef std::vector<DxgiOutputInfo> DxgiOutputInfoList;

/**
 * @brief Convert rotation enum to friendly string
 *
 * @param rotation  enum DXGI_MODE_ROTATION
 *
 * @return Rotation friendly description
 *
 */
static std::string rotation_desc(const DXGI_MODE_ROTATION rotation)
{
    switch (rotation) {
    case DXGI_MODE_ROTATION_UNSPECIFIED:    return "Unspecified";
    case DXGI_MODE_ROTATION_IDENTITY:       return "Identity";
    case DXGI_MODE_ROTATION_ROTATE90:       return "Rotate 90";
    case DXGI_MODE_ROTATION_ROTATE180:      return "Rotate 180";
    case DXGI_MODE_ROTATION_ROTATE270:      return "Rotate 270";
    }
    return "Unknown";
};

/**
 * DXGI Adapter Information
 */
struct DxgiAdapterInfo
{
  uint32_t adapterIndex;            //!< adapter index
  DXGI_ADAPTER_DESC1 desc;          //!< DXGI adapter description
  DxgiOutputInfoList outputInfo;    //!< list of output infomation
};
typedef std::vector<DxgiAdapterInfo> DxgiAdapterInfoList;

/**
 * @brief Show mode
 *
 */
enum ShowMode {
    off = 0,
    basic = 1,
    details = 2
};

/**
 * @brief Enumeration use API
 *
 */
enum ApiToUse {
    dxgi = 0,
    d3dkmt = 1
};

/**
 * @brief Display app's usage
 *
 * @param app The application name
 *
 * @return none.
 *
 */
void usage(const char* app)
{
  std::cout << "Build Version: " << CG_VERSION << std::endl << std::endl;
  std::cout << std::endl;
  std::cout << "usage: " << app << " [OPTIONS]" << std::endl;
  std::cout << std::endl;
  std::cout << "  options:" << std::endl;
  std::cout << "    --help                          Display this help and exit" << std::endl;
  std::cout << "    --api   dxgi | d3dkmt           Enumeration API to use (default: dxgi)" << std::endl;
  std::cout << "    --show  basic | details | off   How much informaiton to show (default: basic)" << std::endl;
  std::cout << "    --debug                         Show debug message" << std::endl;
  std::cout << "    --luid  high:low                Specifies adapter LUID \"high:low\" to get adapter index" << std::endl;
  std::cout << "                                    \"high:low\" in decimal #### or hexadecimal 0xXXXX format" << std::endl;
}

/**
 * main function
 */
int main(int argc, char** argv)
{
    int idx = 1;
    ShowMode show_mode = ShowMode::basic;
    ApiToUse use_api = ApiToUse::dxgi;
    bool debug = false;
    const char* arg_luid = nullptr;

    // parsing command line argument options
    if (argc > 1) {
        for (idx = 1; idx < argc; ++idx) {
            if (std::string("-h") == argv[idx] || std::string("--help") == argv[idx]) {
                usage(argv[0]);
                return 0;
            }
            else if (std::string("--api") == argv[idx]) {
                if (!(++idx >= argc)) {
                    if (std::string("dxgi") == argv[idx])
                        use_api = ApiToUse::dxgi;
                    else if (std::string("d3dkmt") == argv[idx])
                        use_api = ApiToUse::d3dkmt;
                    else
                        std::cout << "WARNING: Unknow '--api' argument - '" << argv[idx] << "'. Use default 'dxgi' list." << std::endl;
                }
            }
            else if (std::string("--luid") == argv[idx]) {
                if (++idx >= argc) {
                    std::cerr << "ERROR: Missing LUID argumnet for '--luid'" << std::endl;
                    std::cout << std::endl;
                    usage(argv[0]);
                    return -1;
                }
                arg_luid = argv[idx];
                show_mode = ShowMode::off;
            }
            else if (std::string("--show") == argv[idx]) {
                show_mode = ShowMode::basic;
                if (!(++idx >= argc)) {
                    if (std::string("details") == argv[idx])
                        show_mode = ShowMode::details;
                    else if (std::string("basic") == argv[idx])
                        show_mode = ShowMode::basic;
                    else
                        std::cout << "WARNING: Unknow '--show' argument - '" << argv[idx] << "'. Use default 'basic' list." << std::endl;
                }
            }
            else if (std::string("--debug") == argv[idx]) {
                debug = true;
            }
            else {
                std::cerr << "ERROR: Unknown argument optoin: " << argv[idx] << std::endl;
                std::cout << std::endl;
                usage(argv[0]);
                return -1;
            }
        }
    }

    if (use_api == ApiToUse::dxgi) {
        DxgiAdapterInfoList gpuAdapterDescs;

        CComPtr<IDXGIFactory6> dxgiFactory;
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr) || !dxgiFactory) {
            std::cerr << "Failed to create DXGI factory!!! " << HrToString(hr) << std::endl;
            return -1;
        }

        // Enumerating adapters information
        CComPtr<IDXGIAdapter1> adapter;

        for (UINT adapterIdx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(adapterIdx, &adapter); ++adapterIdx) {
            DxgiAdapterInfo adapterInfo{};
            ThrowIfFailed(adapter->GetDesc1(&adapterInfo.desc));
            adapterInfo.adapterIndex = adapterIdx;

            // Enumerating output from the adapter
            CComPtr<IDXGIOutput> output = nullptr;
            uint32_t outputIdx = 0;
            while (SUCCEEDED(adapter->EnumOutputs(outputIdx, &output))) {
                DxgiOutputInfo outputInfo{};
                ThrowIfFailed(output->GetDesc(&outputInfo.desc));
                outputInfo.outputIndex = outputIdx;

                if (outputInfo.desc.AttachedToDesktop) {
                    RtlZeroMemory(&outputInfo.monitorInfo, sizeof(outputInfo.monitorInfo));
                    outputInfo.monitorInfo.cbSize = sizeof(outputInfo.monitorInfo);
                    if (!GetMonitorInfoW(outputInfo.desc.Monitor, &outputInfo.monitorInfo)) {
                        std::cerr << "Failed to get monitor information! (adapter index = " << adapterInfo.adapterIndex << ", output index = " << outputInfo.outputIndex << std::endl;
                    }
                }
                adapterInfo.outputInfo.push_back(std::move(outputInfo));

                output.Release();
                ++outputIdx;
            }

            gpuAdapterDescs.push_back(std::move(adapterInfo));

            adapter.Release();
        }

        // Show list of GPU adapters
        if (show_mode != ShowMode::off && !gpuAdapterDescs.empty()) {
            std::cout << std::endl;
            std::cout << "[DXGI] Total number of D3D/GPU adapters: = " << gpuAdapterDescs.size() << ":" << std::endl;
            std::cout << std::endl;

            for (auto& adapterInfo : gpuAdapterDescs) {
                std::cout << "\tAdapter index #" << adapterInfo.adapterIndex << std::endl;
                std::cout << "\t----------------" << std::endl;
                std::wcout << "\t   Description : " << std::wstring(adapterInfo.desc.Description) << std::endl;
                std::cout << "\t     Vendor ID : " << std::to_string(adapterInfo.desc.VendorId) << " [0x" << std::hex << adapterInfo.desc.VendorId << std::dec << "]" << std::endl;
                std::cout << "\t     Device ID : " << std::to_string(adapterInfo.desc.DeviceId) << " [0x" << std::hex << adapterInfo.desc.DeviceId << std::dec << "]" << std::endl;
                std::cout << "\t  Subsystem ID : " << std::to_string(adapterInfo.desc.SubSysId) << " [0x" << std::hex << adapterInfo.desc.SubSysId << std::dec << "]" << std::endl;
                std::cout << "\t      Revision : " << std::to_string(adapterInfo.desc.Revision) << " [0x" << std::hex << adapterInfo.desc.Revision << std::dec << "]" << std::endl;
                std::cout << "\t  Adapter LUID : " << std::to_string(adapterInfo.desc.AdapterLuid.HighPart)
                    << " " << std::to_string(adapterInfo.desc.AdapterLuid.LowPart)
                    << " [0x" << std::hex << adapterInfo.desc.AdapterLuid.HighPart
                    << " 0x" << adapterInfo.desc.AdapterLuid.LowPart << std::dec << "]" << std::endl;

                for (auto& outputInfo : adapterInfo.outputInfo) {
                    std::cout << "\n\t\tOutput index #" << outputInfo.outputIndex << std::endl;
                    std::cout << "\t\t----------------" << std::endl;
                    std::wcout << "\t\t           Device Name : " << std::wstring(outputInfo.desc.DeviceName) << std::endl;
                    std::cout << "\t\t   Attached To Desktop : " << (outputInfo.desc.AttachedToDesktop ? "Yes" : "No") << std::endl;
                    std::cout << "\t\t   Desktop Coordinates : (" << outputInfo.desc.DesktopCoordinates.left
                        << ", " << outputInfo.desc.DesktopCoordinates.top
                        << ", " << outputInfo.desc.DesktopCoordinates.right
                        << ", " << outputInfo.desc.DesktopCoordinates.bottom << ")" << std::endl;
                    std::cout << "\t\t              Rotation : " << rotation_desc(outputInfo.desc.Rotation) << std::endl;
                    std::cout << "\t\t     Handle of Monitor : 0x" << std::hex << outputInfo.desc.Monitor << std::dec << std::endl;
                    std::wcout << "\t\t   Monitor Device Name : " << std::wstring(outputInfo.monitorInfo.szDevice) << std::endl;
                }
                std::cout << std::endl;
            }
        }

        if (arg_luid != nullptr) {
            std::string luidstr = std::string(arg_luid);
            LUID luid;
            bool found = false;

            if (!check_luid(luidstr, luid)) {
                return -1;
            }

            auto compare_luid = [&luid](DxgiAdapterInfo& info) {
                return (info.desc.AdapterLuid.HighPart == luid.HighPart && info.desc.AdapterLuid.LowPart == luid.LowPart);
            };

            if (gpuAdapterDescs.size() > 0) {
                auto it = std::find_if(gpuAdapterDescs.begin(), gpuAdapterDescs.end(), compare_luid);
                if (it != gpuAdapterDescs.end()) {
                    int index = (int)std::distance(gpuAdapterDescs.begin(), it);
                    std::cout << index << std::endl;    // echo found LUID adapter index for automation script use
                    found = true;
                }
            }

            if (!found) {
                std::cerr << "LUID [" << luidstr << "] not found!" << std::endl;
            }
        }
    }
    else if (use_api == ApiToUse::d3dkmt) {
        AdapterDeviceInfoList physicalDevices;   // List of physical adapter device information
        AdapterDeviceInfoList softwareDevices;   // List of software adapter device information
        AdapterDeviceInfoList indirectDevices;   // List of indirect display adapter device information

        query_adapters_list(physicalDevices, indirectDevices, softwareDevices, debug);

        if (show_mode != ShowMode::off) {
            std::cout << "[D3DKMT] Total number of D3D/GPU adapters = " << (physicalDevices.size() + indirectDevices.size() + softwareDevices.size()) << std::endl;
            std::cout << std::endl;

            std::cout << "Number of Physical Devices  : " << physicalDevices.size() << std::endl;
            std::cout << "================================" << std::endl << std::endl;
            int i = 0;
            for (auto& info : physicalDevices) {
                std::wcout << L"Adapter Index in Physical Devices List [" << i++ << L"]" << std::endl;
                std::wcout << L"-------------------------------------------" << std::endl;
                show_adapter_device_info(info, show_mode == ShowMode::details);
            }

            std::cout << "Number of Indirect Devices  : " << indirectDevices.size() << std::endl;
            std::cout << "================================" << std::endl << std::endl;
            i = 0;
            for (auto& info : indirectDevices) {
                std::wcout << L"Adapter Index in Indirect Devices List [" << i++ << L"]" << std::endl;
                std::wcout << L"-------------------------------------------" << std::endl;
                show_adapter_device_info(info, show_mode == ShowMode::details);
            }

            std::cout << "Number of Software Devices  : " << softwareDevices.size() << std::endl;
            std::cout << "================================" << std::endl << std::endl;
            i = 0;
            for (auto& info : softwareDevices) {
                std::wcout << L"Adapter Index in Software Devices List [" << i++ << L"]" << std::endl;
                std::wcout << L"-------------------------------------------" << std::endl;
                show_adapter_device_info(info, show_mode == ShowMode::details);
            }
        }

        if (arg_luid != nullptr) {
            std::string luidstr = std::string(arg_luid);
            LUID luid;
            bool found = false;

            if (!check_luid(luidstr, luid)) {
                return -1;
            }

            auto compare_luid = [&luid](AdapterDeviceInfo& info) {
                return (info.luid.HighPart == luid.HighPart && info.luid.LowPart == luid.LowPart);
            };

            // Check physical adapters list first
            if (physicalDevices.size() > 0) {
                auto it = std::find_if(physicalDevices.begin(), physicalDevices.end(), compare_luid);
                if (it != physicalDevices.end()) {
                    int index = (int)std::distance(physicalDevices.begin(), it);
                    if (debug) {
                        std::cout << "Found [" << luidstr << "] in the physical devices. index = " << index << std::endl;
                        show_adapter_device_info(physicalDevices[index], show_mode == ShowMode::details);
                    }
                    else {
                        std::cout << index << std::endl;    // echo found LUID adapter index for automation script use
                    }
                    found = true;
                }
            }
            // If not found in physical adapters list check indirect display adapters list
            if (!found && indirectDevices.size() > 0) {
                auto it = std::find_if(indirectDevices.begin(), indirectDevices.end(), compare_luid);
                if (it != indirectDevices.end()) {
                    int index = (int)std::distance(indirectDevices.begin(), it);
                    if (debug) {
                        std::cout << "Found [" << luidstr << "] in the indirect devices. index = " << index << std::endl;
                        show_adapter_device_info(indirectDevices[index], show_mode == ShowMode::details);
                    }
                    else {
                        std::cout << index << std::endl;    // echo found LUID adapter index for automation script use
                    }
                    found = true;
                }
            }
            // If not found in neither physical nor indirect adapters list, check software adapters list
            if (!found && softwareDevices.size() > 0) {
                auto it = std::find_if(softwareDevices.begin(), softwareDevices.end(), compare_luid);
                if (it != softwareDevices.end()) {
                    int index = (int)std::distance(softwareDevices.begin(), it);
                    if (debug) {
                        std::cout << "Found [" << luidstr << "] in the software devices. index = " << index << std::endl;
                        show_adapter_device_info(softwareDevices[index], show_mode == ShowMode::details);
                    }
                    else {
                        std::cout << index << std::endl;    // echo found LUID adapter index for automation script use
                    }
                    found = true;
                }
            }

            if (!found) {
                std::cerr << "LUID [" << luidstr << "] not found!" << std::endl;
            }
        }
    }

    return 0;
}
