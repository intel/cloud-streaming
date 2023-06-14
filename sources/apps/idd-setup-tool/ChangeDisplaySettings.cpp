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

#include "ChangeDisplaySettings.h"

#include <sstream>
#include <algorithm>

using namespace std;

static DPIScalingInfo GetDPIScalingInfo(LUID adapterID, UINT32 sourceID)
{
    DISPLAYCONFIG_SOURCE_DPI_SCALE_GET requestPacket = {};
    requestPacket.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DISPLAYCONFIG_DEVICE_INFO_TYPE_CUSTOM::DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE;
    requestPacket.header.size = sizeof(requestPacket);
    requestPacket.header.adapterId = adapterID;
    requestPacket.header.id = sourceID;

    auto res = DisplayConfigGetDeviceInfo(&requestPacket.header);
    if (res != ERROR_SUCCESS)
    {
        tstringstream ss;
        ss << "ERROR: DisplayConfigGetDeviceInfo failed with status " << res << '\n';
        throw ss.str();
    }

    requestPacket.curScaleRel = clamp(requestPacket.curScaleRel, requestPacket.minScaleRel, requestPacket.maxScaleRel);

    int32_t min_abs = abs(requestPacket.minScaleRel);
    if (size(known_dpi) < size_t(min_abs) + requestPacket.maxScaleRel + 1)
    {
        tstringstream ss;
        ss << "ERROR: Invalid index for known DPI array " << (size_t(min_abs) + requestPacket.maxScaleRel)
            << " while max idx is " << size_t(size(known_dpi) - 1) << '\n';
        throw ss.str();
    }

    DPIScalingInfo dpi_info = {};

    dpi_info.current = known_dpi[min_abs + requestPacket.curScaleRel];
    dpi_info.recommended = known_dpi[min_abs];
    dpi_info.maximum = known_dpi[min_abs + requestPacket.maxScaleRel];

    return dpi_info;
}

static void SetDPIScaling(LUID adapterID, UINT32 sourceID, UINT32 dpi_to_set)
{
    DPIScalingInfo dPIScalingInfo = GetDPIScalingInfo(adapterID, sourceID);

    dpi_to_set = clamp(dpi_to_set, dPIScalingInfo.minimum, dPIScalingInfo.maximum);

    // Everything is already fine, do nothing
    if (dpi_to_set == dPIScalingInfo.current)
        return;

    auto idx_to_set = find(begin(known_dpi), end(known_dpi), dpi_to_set);
    auto idx_recommended = find(begin(known_dpi), end(known_dpi), dPIScalingInfo.recommended);

    if (idx_to_set == end(known_dpi) || idx_recommended == end(known_dpi))
    {
        tstringstream ss;
        ss << "ERROR: cannot find desired DPI value " << dpi_to_set << '\n';
        throw ss.str();
    }

    int64_t dpi_relative = idx_to_set - idx_recommended;

    DISPLAYCONFIG_SOURCE_DPI_SCALE_SET setPacket = {};
    setPacket.header.adapterId = adapterID;
    setPacket.header.id = sourceID;
    setPacket.header.size = sizeof(setPacket);
    setPacket.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DISPLAYCONFIG_DEVICE_INFO_TYPE_CUSTOM::DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE;
    setPacket.scaleRel = (UINT32)dpi_relative;

#if !DRY_RUN
    auto res = DisplayConfigSetDeviceInfo(&setPacket.header);
    if (res != ERROR_SUCCESS)
    {
        tstringstream ss;
        ss << "ERROR: DisplayConfigSetDeviceInfo failed with status " << res << '\n';
        throw ss.str();
    }
#endif
}

void SetDisplayDPI(uint32_t dpi_to_set, bool verbose)
{
    vector<DISPLAYCONFIG_PATH_INFO> pathsV;
    vector<DISPLAYCONFIG_MODE_INFO> modesV;

    LONG status;
    do
    {
        UINT32 flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
        UINT32 num_paths = 0, num_modes = 0;

        status = GetDisplayConfigBufferSizes(flags, &num_paths, &num_modes);
        if (status != ERROR_SUCCESS)
        {
            tstringstream ss;
            ss << "ERROR: GetDisplayConfigBufferSizes failed with code " << status << endl;
            throw ss.str();
        }

        if (num_paths == 0 || num_modes == 0)
        {
            tstringstream ss;
            ss << "ERROR: No active display discovered, can't set dpi" << endl;
            throw ss.str();
        }

        pathsV.resize(num_paths);
        modesV.resize(num_modes);

        status = QueryDisplayConfig(flags, &num_paths, pathsV.data(), &num_modes, modesV.data(), nullptr);
        if (status != ERROR_SUCCESS && status != ERROR_INSUFFICIENT_BUFFER)
        {
            tstringstream ss;
            ss << "ERROR: QueryDisplayConfig failed with code " << status << endl;
            throw ss.str();
        }
        pathsV.resize(num_paths);
        modesV.resize(num_modes);
        // It's possible that between the call to GetDisplayConfigBufferSizes and QueryDisplayConfig
        // that the display state changed, so loop on the case of ERROR_INSUFFICIENT_BUFFER.
    } while (status == ERROR_INSUFFICIENT_BUFFER);

    for (const auto& path : pathsV)
    {
        //get display name
        auto adapterLUID = path.targetInfo.adapterId;
        auto sourceID = path.sourceInfo.id;

        DISPLAYCONFIG_TARGET_DEVICE_NAME device_name;
        if (verbose)
        {
            DPIScalingInfo dpi_info = GetDPIScalingInfo(adapterLUID, sourceID);

            device_name.header.size = sizeof(device_name);
            device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            device_name.header.adapterId = adapterLUID;
            device_name.header.id = path.targetInfo.id;

            if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&device_name.header))
            {
                tcout << FormatOutput(L"DisplayConfigGetDeviceInfo() failed") << endl;
            }
            else
            {
                tcout << FormatOutput(L"Device %s:", device_name.monitorFriendlyDeviceName) << endl;
                tcout << FormatOutputWithOffset(1, L"Current Scaling is %u", dpi_info.current) << endl;
            }
            tcout << FormatOutputWithOffset(1, L"Setting Scaling to %u", dpi_to_set) << endl;
        }

        SetDPIScaling(adapterLUID, sourceID, dpi_to_set);

        if (verbose)
        {
            DPIScalingInfo dpi_info = GetDPIScalingInfo(adapterLUID, sourceID);
            tcout << FormatOutputWithOffset(1, L"Current Scaling for device %s is %u", device_name.monitorFriendlyDeviceName, dpi_info.current) << endl;
        }
    }
}

void SetDisplayResolution(uint32_t width, uint32_t height)
{
    DISPLAY_DEVICE displayDevice = {};
    displayDevice.cb = sizeof(displayDevice);

    for(int sourceID = 0; EnumDisplayDevices(NULL, sourceID, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME) != 0; sourceID++) {
        if (EnumDisplayDevices(NULL, sourceID, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME) == 0) {
            tstringstream ss;
            ss << "\t\tERROR: EnumDisplayDevices failed for iDevNum=" << sourceID << endl;
            throw ss.str();
        }

        bool targetIsIDDDevice = false;
        if (wstring(displayDevice.DeviceString).compare(wstring(L"Intel IddSampleDriver Device")) == 0) {
            targetIsIDDDevice = true;
        }

        bool targetResolutionIsSupported = false;
        DEVMODE displaySettingsQuery = { 0 };
        displaySettingsQuery.dmSize = sizeof(displaySettingsQuery);

        for(int iModeNum = 0; targetResolutionIsSupported == false && EnumDisplaySettings(displayDevice.DeviceName, iModeNum, &displaySettingsQuery) != 0; iModeNum++) {
            if (displaySettingsQuery.dmPelsWidth == width && displaySettingsQuery.dmPelsHeight == height) {
                targetResolutionIsSupported = true;
            }
        }

        if (targetIsIDDDevice == false) {
            tcout << FormatOutput(L"Display #%d (SKIP): %s (%s) is not an IDD display", sourceID, displayDevice.DeviceString, displayDevice.DeviceName) << endl;
        } else if (targetResolutionIsSupported == false) {
            tcout << FormatOutput(L"Display #%d (SKIP): %s (%s) does not support requested resolution %u by %u", sourceID, displayDevice.DeviceString, displayDevice.DeviceName, width, height) << endl;
        } else {
            DEVMODE lpDevMode = {};
            lpDevMode.dmSize = sizeof(lpDevMode);
            lpDevMode.dmPelsWidth = (DWORD) width;
            lpDevMode.dmPelsHeight = (DWORD) height;
            lpDevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettingsEx(displayDevice.DeviceName, &lpDevMode, NULL, 0, NULL) != DISP_CHANGE_SUCCESSFUL) {
                tcout << FormatOutput(L"Display #%d (FAIL): %s (%s) failed to changed resolution to %u by %u", sourceID, displayDevice.DeviceString, displayDevice.DeviceName, width, height) << endl;
            } else {
                tcout << FormatOutput(L"Display #%d (PASS): %s (%s) resolution changed to %u by %u", sourceID, displayDevice.DeviceString, displayDevice.DeviceName, width, height) << endl;
            }
        }
    }
}
