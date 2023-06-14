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

#include "EnableDisableAdapters.h"

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <setupapi.h>
#include <regstr.h>
#include <infstr.h>
#include <cfgmgr32.h>
#include <string.h>
#include <malloc.h>
#include <newdev.h>
#include <objbase.h>
#include <strsafe.h>
#include <io.h>
#include <fcntl.h>

#include <memory>
#include <sstream>
using namespace std;

#pragma comment (lib , "Setupapi.lib")

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

static void MSG_LISTCLASS_NOCLASS_LOCAL(const tstring& class_name)
{
    tcout << FormatOutputWithOffset(1, L"ERROR: There is no %s setup class on the local machine", class_name.c_str()) << endl;
}

static void MSG_LISTCLASS_HEADER_NONE_LOCAL(const tstring& class_name, const tstring& class_descr)
{
    tcout << FormatOutputWithOffset(1, L"ERROR: There are no devices in setup class %s (%s)", class_name.c_str(), class_descr.c_str()) << endl;
}

static void MSG_LISTCLASS_HEADER_LOCAL(DWORD dev_count, const tstring& class_name, const tstring& class_descr)
{
    tcout << FormatOutputWithOffset(1, L"Listing %lu devices in setup class %s (%s)", dev_count, class_name.c_str(), class_descr.c_str()) << endl;
}

//////////////////////////////////////////////////////////////////////////
// All taken from devcon.cpp from github
//////////////////////////////////////////////////////////////////////////

static string data_type_to_string(DWORD data_type)
{
    switch (data_type)
    {
    case REG_NONE:
        return "REG_NONE";
    case REG_SZ:
        return "REG_SZ";
    case REG_EXPAND_SZ:
        return "REG_EXPAND_SZ";
    case REG_BINARY:
        return "REG_BINARY";
    case REG_DWORD:
        return "REG_DWORD / REG_DWORD_LITTLE_ENDIAN";
    case REG_DWORD_BIG_ENDIAN:
        return "REG_DWORD_BIG_ENDIAN";
    case REG_LINK:
        return "REG_LINK";
    case REG_MULTI_SZ:
        return "REG_MULTI_SZ";
    case REG_RESOURCE_LIST:
        return "REG_RESOURCE_LIST";
    case REG_FULL_RESOURCE_DESCRIPTOR:
        return "REG_FULL_RESOURCE_DESCRIPTOR";
    case REG_RESOURCE_REQUIREMENTS_LIST:
        return "REG_RESOURCE_REQUIREMENTS_LIST";
    case REG_QWORD:
        return "REG_QWORD / REG_QWORD_LITTLE_ENDIAN";
    default:
        return "Unknown";
    }
}

static tstring GetDeviceStringProperty(_In_ HDEVINFO Devs, _In_ PSP_DEVINFO_DATA DevInfo, _In_ DWORD Prop)
{
    vector<TCHAR> buffer((1024 / sizeof(TCHAR)) + 1);

    DWORD reqSize, dataType;
    while (!SetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, &dataType, (LPBYTE)buffer.data(), (DWORD)buffer.size() * sizeof(TCHAR), &reqSize))
    {
        auto err = GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER)
        {
            GetLastErrorAndThrow(TEXT("SetupDiGetDeviceRegistryProperty"));
        }
        if (dataType == REG_SZ) {
            // Do nothing
        } else if (dataType == REG_MULTI_SZ) {
            // Do nothing
        } else {
            tstringstream ss;
            ss << "ERROR: SetupDiGetDeviceRegistryProperty returned data type " << dataType << " (" << data_type_to_string(REG_SZ).c_str() << ") "
                << "but expected type " << REG_SZ << " (REG_SZ) or " << REG_MULTI_SZ << " (REG_MULTI_SZ)\n";
            throw ss.str();
        }
        buffer.resize((reqSize / sizeof(TCHAR)) + 1);
    }
    buffer.back() = TEXT('\0');

    return tstring(begin(buffer), end(buffer));
}

static tstring GetDeviceDescription(_In_ HDEVINFO Devs, _In_ PSP_DEVINFO_DATA DevInfo)
{
    tstring desc;

    try
    {
        desc = GetDeviceStringProperty(Devs, DevInfo, SPDRP_FRIENDLYNAME);
    }
    catch (...)
    {
        desc = GetDeviceStringProperty(Devs, DevInfo, SPDRP_DEVICEDESC);
    }
    return desc;
}

static tstring GetDeviceHardwareID(_In_ HDEVINFO Devs, _In_ PSP_DEVINFO_DATA DevInfo)
{
    return GetDeviceStringProperty(Devs, DevInfo, SPDRP_HARDWAREID);
}

static tstring GetDeviceID(_In_ HDEVINFO Devs, _In_ PSP_DEVINFO_DATA DevInfo)
{
    tstring device_id;
    TCHAR devID[MAX_DEVICE_ID_LEN] = {};

    SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
    devInfoListDetail.cbSize = sizeof(devInfoListDetail);

    if ((!SetupDiGetDeviceInfoListDetail(Devs, &devInfoListDetail)) ||
        (CM_Get_Device_ID_Ex(DevInfo->DevInst, devID, MAX_DEVICE_ID_LEN, 0, devInfoListDetail.RemoteMachineHandle) != CR_SUCCESS))
    {
        StringCchCopy(devID, ARRAYSIZE(devID), TEXT("?"));
    }

    return tstring(devID);
}

static VOID DumpDevice(_In_ HDEVINFO Devs, _In_ PSP_DEVINFO_DATA DevInfo, _In_ Adapter &adapter)
{
    tcout << FormatOutputWithOffset(1, L"Device Name: %s", adapter.devName.c_str()) << endl;
    tcout << FormatOutputWithOffset(2, L"Device ID         : %s", adapter.devID.c_str()) << endl;
    tcout << FormatOutputWithOffset(2, L"Device Hardware ID: %s", adapter.devHardwareID.c_str()) << endl;
    tcout << FormatOutputWithOffset(2, L"Device GUID       : {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
        adapter.devGuid.Data1, adapter.devGuid.Data2, adapter.devGuid.Data3,
        adapter.devGuid.Data4[0], adapter.devGuid.Data4[1], adapter.devGuid.Data4[2], adapter.devGuid.Data4[3],
        adapter.devGuid.Data4[4], adapter.devGuid.Data4[5], adapter.devGuid.Data4[6], adapter.devGuid.Data4[7]) << endl;
    tcout << FormatOutputWithOffset(2, L"Device Index      : %d", adapter.devIndex) << endl;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

static std::unordered_map<tstring, uint32_t> PatternMatchCounts {};
static std::unordered_map<tstring, std::vector<tstring>> DeviceIDsForIndexedPatterns {};

static void CheckEnableDisablePatternMatches(tstring device_name, tstring device_id, bool verbose)
{
    if (verbose) {
        tcout << FormatOutputWithOffset(2, L"Adapter can be enabled and disabled with patterns:") << endl;
    }
    for (EnableDisablePatternStruct& pattern : SupportedEnableDisablePatterns) {
        for (tstring sub_pattern : pattern.AdaptersToMatch) {
            bool matches_this_pattern = false;
            if (pattern.PatternType == ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION &&
                CheckIfStringContainsPattern(device_name, sub_pattern, true) != pattern.IsAnInvertedTarget) {
                matches_this_pattern = true;
            } else if (pattern.PatternType == ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID &&
                CheckIfStringContainsPattern(device_id, sub_pattern, true) != pattern.IsAnInvertedTarget) {
                matches_this_pattern = true;
            }
            if (matches_this_pattern) {
                if (verbose) {
                    tcout << FormatOutputWithOffset(3, L"%s", pattern.Abbreviation.c_str()) << endl;
                }
                if (PatternMatchCounts.find(pattern.Abbreviation) == PatternMatchCounts.end()) {
                    PatternMatchCounts[pattern.Abbreviation] = 1;
                    DeviceIDsForIndexedPatterns[pattern.Abbreviation] = std::vector<tstring>();
                } else {
                    PatternMatchCounts[pattern.Abbreviation]++;
                }
                DeviceIDsForIndexedPatterns[pattern.Abbreviation].push_back(device_id);
                if (verbose) {
                    tcout << FormatOutputWithOffset(3, L"%s%u", pattern.Abbreviation.c_str(), PatternMatchCounts[pattern.Abbreviation]) << endl;
                }
                break;
            }
        }
    }
}

std::vector<Adapter> GetAdapterList(bool verbose)
{
    vector<Adapter> out_list;

    PatternMatchCounts.clear();
    DeviceIDsForIndexedPatterns.clear();

    vector<tstring> k_ARGS = { TEXT("Display") };

    for (auto& arg : k_ARGS)
    {
        if (arg.empty())
            continue;

        //
        // there could be one to many name to GUID mapping
        //
        vector<GUID> guids(16);
        DWORD num_guids = 0;
        while (!SetupDiClassGuidsFromNameEx(arg.data(), guids.data(), (DWORD)guids.size(), &num_guids, NULL, NULL))
        {
            auto err = GetLastError();
            if (err != ERROR_INSUFFICIENT_BUFFER)
            {
                GetLastErrorAndThrow(TEXT("SetupDiClassGuidsFromNameEx"));
            }
            guids.resize(num_guids);
        }
        guids.resize(num_guids);

        if (guids.empty())
        {
            if (verbose)
                MSG_LISTCLASS_NOCLASS_LOCAL(arg);

            continue;
        }

        for (auto& guid : guids)
        {
            SP_DEVINFO_DATA devInfo;

            HDEVINFO devs = SetupDiGetClassDevsEx(&guid, NULL, NULL, DIGCF_PRESENT, NULL, NULL, NULL);

            if (devs == INVALID_HANDLE_VALUE)
            {
                GetLastErrorAndThrow(TEXT("SetupDiGetClassDevsEx"));
            }

            ClassDevsScopedStorage scoped_devs(devs);

            //
            // count number of devices
            //
            devInfo.cbSize = sizeof(devInfo);
            DWORD devCount = 0;
            for (; SetupDiEnumDeviceInfo(devs, devCount, &devInfo); ++devCount)
            {
            }

            TCHAR className[MAX_CLASS_NAME_LEN] = {};
            if (!SetupDiClassNameFromGuidEx(&guid, className, MAX_CLASS_NAME_LEN, NULL, NULL, NULL))
            {
                if (FAILED(StringCchCopy(className, MAX_CLASS_NAME_LEN, TEXT("?"))))
                {
                    GetLastErrorAndThrow(TEXT("SetupDiClassNameFromGuidEx"));
                }
            }

            TCHAR classDesc[LINE_LEN] = {};
            if (!SetupDiGetClassDescriptionEx(&guid, classDesc, LINE_LEN, NULL, NULL, NULL))
            {
                if (FAILED(StringCchCopy(classDesc, LINE_LEN, className)))
                {
                    GetLastErrorAndThrow(TEXT("SetupDiGetClassDescriptionEx"));
                }
            }

            //
            // how many devices?
            //
            if (!devCount)
            {
                MSG_LISTCLASS_HEADER_NONE_LOCAL(className, classDesc);
                continue;
            }

            // only if we want info output
            if (verbose)
                MSG_LISTCLASS_HEADER_LOCAL(devCount, className, classDesc);

            for (int devIndex = 0; SetupDiEnumDeviceInfo(devs, devIndex, &devInfo); devIndex++)
            {
                Adapter a;
                a.devID = GetDeviceID(devs, &devInfo);
                a.devName = GetDeviceDescription(devs, &devInfo);
                a.devHardwareID = GetDeviceHardwareID(devs, &devInfo);
                a.devGuid = guid;
                a.devIndex = devIndex;

                out_list.push_back(a);

                if (verbose) {
                    DumpDevice(devs, &devInfo, out_list.back());
                }
                CheckEnableDisablePatternMatches(out_list.back().devName, out_list.back().devID, verbose);
            }
        }
    }

    return out_list;
}

size_t GetNumIddCompatibleAdapters(std::vector<Adapter> &adapter_list)
{
    size_t num_idd_compatible_adapters = 0;
    for (auto& adapter : adapter_list) {
        tstring hardware_id = StringToLowerCase(adapter.devHardwareID);
        if (
            hardware_id.find(tstring(TEXT("ven_8086"))) != tstring::npos &&
            hardware_id.find(tstring(TEXT("dev_56c1"))) != tstring::npos
        ) {
            num_idd_compatible_adapters++;
        } else if (
            hardware_id.find(tstring(TEXT("ven_8086"))) != tstring::npos &&
            hardware_id.find(tstring(TEXT("dev_56c0"))) != tstring::npos
        ) {
            num_idd_compatible_adapters++;
        }
    }
    return num_idd_compatible_adapters;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

static void ChangeAdapterState(const Adapter& dev, bool bEnable, bool verbose = true)
{
    HDEVINFO devs = SetupDiGetClassDevsEx(&dev.devGuid, NULL, NULL, DIGCF_PRESENT, NULL, NULL, NULL);

    if (devs == INVALID_HANDLE_VALUE)
    {
        GetLastErrorAndThrow(TEXT("SetupDiGetClassDevsEx"));
    }

    ClassDevsScopedStorage scoped_devs(devs);

    SP_DEVINFO_DATA DevInfo;
    DevInfo.cbSize = sizeof(DevInfo);

    bool bFound = false;
    for (int devIndex = 0; SetupDiEnumDeviceInfo(devs, devIndex, &DevInfo); devIndex++)
    {
        tstring thisId = GetDeviceID(devs, &DevInfo);
        if (dev.devID.compare(thisId) == 0)
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        tstringstream ss;
        ss << "ERROR: couldn't find device " << dev.devName << " [ID: " << dev.devID << ", idx: " << dev.devIndex << "]\n";

        throw ss.str();
    }

    SP_PROPCHANGE_PARAMS pcp;
    SP_DEVINSTALL_PARAMS devParams;
    if (bEnable)
    {
        //
        // enable both on global and config-specific profile
        // do global first and see if that succeeded in enabling the device
        // (global enable doesn't mark reboot required if device is still
        // disabled on current config whereas vice-versa isn't true)
        //
        pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pcp.StateChange = DICS_ENABLE;
        pcp.Scope = DICS_FLAG_GLOBAL;
        pcp.HwProfile = 0;
        //
        // don't worry if this fails, we'll get an error when we try config-
        // specific.
#if !DRY_RUN
        if (SetupDiSetClassInstallParams(devs, &DevInfo, &pcp.ClassInstallHeader, sizeof(pcp)))
        {
            SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devs, &DevInfo);
        }
#endif
    }

    // operate on config-specific profile
    pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    pcp.StateChange = bEnable ? DICS_ENABLE : DICS_DISABLE;
    pcp.Scope = DICS_FLAG_CONFIGSPECIFIC;
    pcp.HwProfile = 0;

#if !DRY_RUN
    if (SetupDiSetClassInstallParams(devs, &DevInfo, &pcp.ClassInstallHeader, sizeof(pcp)))
    {
        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devs, &DevInfo))
        {
            GetLastErrorAndThrow(TEXT("SetupDiCallClassInstaller"));
        }
    }
    else
    {
        GetLastErrorAndThrow(TEXT("SetupDiSetClassInstallParams"));
    }
#endif

    // see if device needs reboot
    devParams.cbSize = sizeof(devParams);

#if !DRY_RUN
    if (verbose)
    {
        if (SetupDiGetDeviceInstallParams(devs, &DevInfo, &devParams))
        {
            tcout << FormatOutputWithOffset(3, L"State change successful. Reboot is %s", ((devParams.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT)) ? L"REQUIRED" : L"NOT REQUIRED")) << endl;
        }
        else
        {
            GetLastErrorAndThrow(TEXT("SetupDiGetDeviceInstallParams"));
        }
    }
#endif
}

void EnableDisableAdapter(vector<Adapter>& adapter_list, adapter_target_info_t& target_info, bool enable, bool verbose)
{
    tstring enable_keyword = tstring((enable ? TEXT("enable") : TEXT("disable")));
    tstring invert_target_keyword = tstring((target_info.is_an_inverted_target ? TEXT("dont match") : TEXT("match")));

    if (verbose) {
        tcout << FormatOutputWithOffset(1, L"Target Name: %s", target_info.pattern.c_str()) << endl;
        tcout << FormatOutputWithOffset(1, L"Invert: %s", invert_target_keyword.c_str()) << endl;
        tcout << FormatOutputWithOffset(1, L"Enable: %s", enable_keyword.c_str()) << endl;
        tcout << FormatOutputWithOffset(1, L"Specific Device: %s", target_info.target_device_id.c_str()) << endl;
        tcout << FormatOutputWithOffset(1, L"Devices:") << endl;
    }

    for (auto& adapter : adapter_list)
    {
        if (verbose) {
            tcout << FormatOutputWithOffset(2, L"Device Name: %s", adapter.devName.c_str()) << endl;
            tcout << FormatOutputWithOffset(3, L"Device ID         : %s", adapter.devID.c_str()) << endl;
            tcout << FormatOutputWithOffset(3, L"Device Hardware ID: %s", adapter.devHardwareID.c_str()) << endl;
            tcout << FormatOutputWithOffset(3, L"Device GUID       : {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                adapter.devGuid.Data1, adapter.devGuid.Data2, adapter.devGuid.Data3,
                adapter.devGuid.Data4[0], adapter.devGuid.Data4[1], adapter.devGuid.Data4[2], adapter.devGuid.Data4[3],
                adapter.devGuid.Data4[4], adapter.devGuid.Data4[5], adapter.devGuid.Data4[6], adapter.devGuid.Data4[7]) << endl;
            tcout << FormatOutputWithOffset(3, L"Device Index      : %d", adapter.devIndex) << endl;
        }

        bool is_match = false;
        if (target_info.pattern_type == ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION && 
            CheckIfStringContainsPattern(adapter.devName, target_info.pattern, true) != target_info.is_an_inverted_target) {
            is_match = true;
        } else if (target_info.pattern_type == ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID && 
            CheckIfStringContainsPattern(adapter.devID, target_info.pattern, true) != target_info.is_an_inverted_target) {
            is_match = true;
        }
        
        if (is_match) {
            if (target_info.target_device_id.length() > 0 && target_info.target_device_id.find(adapter.devID) != 0) {
                // We aren't the specific display adapter requested. Skip this display adapter.
                continue;
            }

            if (verbose) {
                tcout << FormatOutputWithOffset(3, L"Attempting to %s %s", enable_keyword.c_str(), adapter.devName.c_str()) << endl;
            } else {
                tcout << FormatOutput(L"Attempting to %s %s [ID: %s, idx: %d]", enable_keyword.c_str(), adapter.devName.c_str(), adapter.devID.c_str(), adapter.devIndex) << endl;
            }

            ChangeAdapterState(adapter, enable, verbose);
        }
    }
}

void EnableDisableDisplayAdapterManager(tstring Pattern, bool verbose, bool enable)
{
    Pattern = StringToLowerCase(Pattern);

    tstring temp_string = TEXT("disable");
    if (enable) {
        temp_string = TEXT("enable");
    }

    vector<Adapter> adapter_list = GetAdapterList(verbose);

    for (EnableDisablePatternStruct& possible_pattern_match : SupportedEnableDisablePatterns) {
        bool is_a_full_match = false;
        if (Pattern.find(possible_pattern_match.Abbreviation) == 0) {
            is_a_full_match = true;
            tstring target_device_id = TEXT("");
            int32_t target_index = 0;
            // We match this abbreviated pattern. Lets check if it has an index on it.
            if (Pattern.length() > possible_pattern_match.Abbreviation.length()) {
                // We might have an index. Lets verify.
                bool has_index_string = true;
                tstring index_string = tstring(Pattern.begin() + possible_pattern_match.Abbreviation.size(), Pattern.end());
                for (auto character : index_string) {
                    if (tstring(TEXT("0123456789")).find(character) == tstring::npos) {
                        // Oops what we though was an index has non-numeric contents. Guess we didnt match...
                        has_index_string = false;
                    }
                }
                if (has_index_string) {
                    // We have an index. Lets extract it.
                    target_index = get_int_from_tstring(index_string);
                    // Check if the index we got is valid. If not we will alert user and continue.
                    if (target_index < 1 || target_index > PatternMatchCounts[possible_pattern_match.Abbreviation]) {
                        tcout << FormatOutputWithOffset(1, 
                        L"The specified display adapter to %s '%s' matches a valid display adapter pattern '%s' but index '%d' is out of range (max of '%d', min of '1').",
                        temp_string.c_str(),
                        Pattern.c_str(),
                        possible_pattern_match.Abbreviation.c_str(),
                        target_index,
                        PatternMatchCounts[possible_pattern_match.Abbreviation]) << endl;
                        is_a_full_match = false;
                    } else {
                        target_device_id = DeviceIDsForIndexedPatterns[possible_pattern_match.Abbreviation][target_index - 1];
                    }
                } else {
                    is_a_full_match = false;
                }
            }

            if (is_a_full_match) {
                for (tstring sub_pattern : possible_pattern_match.AdaptersToMatch) {
                    adapter_target_info_t target_info;
                    target_info.pattern = sub_pattern;
                    target_info.is_an_inverted_target = possible_pattern_match.IsAnInvertedTarget;
                    target_info.target_device_id = target_device_id;
                    target_info.pattern_type = possible_pattern_match.PatternType;

                    if (enable) {
                        EnableDisableAdapter(adapter_list, target_info, true, verbose);
                    } else {
                        EnableDisableAdapter(adapter_list, target_info, false, verbose);
                    }
                }
                return;
            }
        }
    }

    tcout << FormatOutputWithOffset(1, 
        L"The specified display adapter to %s '%s' matched no display adapter patterns supported by this tool.", temp_string.c_str(), Pattern.c_str()) << endl;
}

void DisableDisplayAdapter(tstring Pattern, bool verbose)
{
    EnableDisableDisplayAdapterManager(Pattern, verbose, false);
}

void EnableDisplayAdapter(tstring Pattern, bool verbose)
{
    EnableDisableDisplayAdapterManager(Pattern, verbose, true);
}
