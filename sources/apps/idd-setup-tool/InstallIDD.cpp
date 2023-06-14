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

#include "InstallIDD.h"
#include "Guids.h"

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <strsafe.h>
#include <tchar.h>
#include <newdev.h>
#include <stdlib.h>
#include <sstream>
#include <filesystem>

using namespace std;

// function pointer - definition from newdev.h, implemented in newdev.dll
typedef BOOL(WINAPI* updateDriverFN)(
    _In_opt_  HWND hwndParent,
    _In_      LPCWSTR HardwareId,
    _In_      LPCWSTR FullInfPath,
    _In_      DWORD InstallFlags,
    _Out_opt_ PBOOL bRebootRequired
    );

bool TrustIDD(const std::filesystem::path& inf_path)
{
    // We will be looking for install_certificate.ps1 script next to the current executable
    std::filesystem::path ps1 = getExePath().replace_filename("install_certificate.ps1");
    std::filesystem::path cat = inf_path / "iddsampledriver.cat";

    auto patharg = [](const std::filesystem::path& path) -> tstring {
        return tstring(L"'") + path.c_str() + tstring(L"'");
    };

    const tstring command = L"powershell -NoProfile -ExecutionPolicy Unrestricted -Command \"& " +
        patharg(ps1) + L" -driverFile " + patharg(cat) + L"\"";

    tstring results;
    bool SystemCallStatus = RunSystemCommand(command, results);

    if (!SystemCallStatus) {
        tcout << FormatOutput(L"Error: %s", command.c_str()) << endl;
        return false;
    }

    tcout << FormatOutput(L"%s", results.c_str()) << endl;
    return true;
}

std::vector<tstring> GetMultiSzDevProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD propId)
{
    std::vector<tstring> prop;
    DWORD propType = 0;
    DWORD propSize = 0;
    if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, propId, &propType, nullptr, 0, &propSize)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || propType != REG_MULTI_SZ || !propSize) {
            return prop;
        }
    }

    tstring buf(propSize, TEXT('\0'));
    if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, propId, nullptr, (PBYTE)buf.data(), buf.size(), nullptr))
        return prop;

    // See: https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-value-types
    // Example: String1\0String2\0String3\0LastString\0\0
    for (auto it = buf.data(); *it != TEXT('\0');) {
        tstring tmp(it);
        it += tmp.size() + 1;
        prop.push_back(tmp);
    }
    return prop;
}

bool UninstallIDD()
{
    HDEVINFO hDevInfo = SetupDiGetClassDevsEx(&DISPLAY_GUID, nullptr, nullptr, DIGCF_PRESENT, nullptr, nullptr, nullptr);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        tcout << FormatOutput(L"Error: SetupDiGetClassDevsEx() failed") << endl;
        return false;
    }

    bool result = true;
    DWORD count = 0;
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD idx = 0; SetupDiEnumDeviceInfo(hDevInfo, idx, &devInfoData); ++idx) {
        std::vector<tstring> hwIds = GetMultiSzDevProperty(hDevInfo, devInfoData, SPDRP_HARDWAREID);

        if (std::find(std::begin(hwIds), std::end(hwIds), TEXT("root\\iddsampledriver")) == std::end(hwIds))
            continue;

        tstring devId(MAX_DEVICE_ID_LEN + 1, TEXT('\0'));
        // We need this id for the better print out of operation results.
        CM_Get_Device_ID(devInfoData.DevInst, devId.data(), devId.size(), 0);

        SP_REMOVEDEVICE_PARAMS rmDevParams{};
        rmDevParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        rmDevParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
        rmDevParams.Scope = DI_REMOVEDEVICE_GLOBAL;
        rmDevParams.HwProfile = 0;

        if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData, &rmDevParams.ClassInstallHeader, sizeof(SP_REMOVEDEVICE_PARAMS)) ||
            !SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo, &devInfoData)) {
            tcout << FormatOutput(L"    ") << devId << ": Remove failed"<< endl;
            result = false;
        } else {
            SP_DEVINSTALL_PARAMS devParams{};
            devParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
            if (SetupDiGetDeviceInstallParams(hDevInfo, &devInfoData, &devParams) &&
                (devParams.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT))) {
                tcout << FormatOutput(L"    ") << devId.c_str() << ": Removed on reboot" << endl;

            } else {
                tcout << FormatOutput(L"    ") << devId.c_str() << ": Removed" << endl;
            }
            ++count;
        }
    }
    tcout << FormatOutput(L"    ") << count << L" device(s) were removed." << endl;

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return result;
}

bool InstallIDD(const std::filesystem::path& inf_path, const tstring& HWID, bool& rebootRequired, bool verbose)
{
    std::filesystem::path inf = inf_path / "IddSampleDriver.inf";

    if (HWID.empty())
    {
        tstringstream ss;
        ss << "ERROR: Empty .inf path\n";
        throw ss.str();
    }

    // retrieve class name and GUID from inf file
    GUID classGUID;
    vector<TCHAR> className(MAX_CLASS_NAME_LEN);
    if (!SetupDiGetINFClass(inf.c_str(), &classGUID, className.data(), (DWORD)size(className), 0))
    {
        GetLastErrorAndThrow(TEXT("SetupDiGetINFClass"));
    }

    // create empty device information set for the class GUID
    HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(&classGUID, 0);
    if (devInfoSet == INVALID_HANDLE_VALUE)
    {
        GetLastErrorAndThrow(TEXT("SetupDiCreateDeviceInfoList"));
    }

    ClassDevsScopedStorage scoped_devs(devInfoSet);

    // create device info element and add to devInfoSet
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo(devInfoSet, className.data(), &classGUID, NULL, 0, DICD_GENERATE_ID, &devInfoData))
    {
        GetLastErrorAndThrow(TEXT("SetupDiCreateDeviceInfo"));
    }

    // copy HWID to string terminated with 00
    tstring HWID_tmp(HWID);
    HWID_tmp += TEXT("\0\0");

    // set PNP device property "SPDRP_HARDWAREID"
    if (!SetupDiSetDeviceRegistryProperty(devInfoSet, &devInfoData, SPDRP_HARDWAREID, (const LPBYTE)HWID_tmp.data(), (DWORD)HWID_tmp.size() * sizeof(TCHAR)))
    {
        GetLastErrorAndThrow(TEXT("SetupDiSetDeviceRegistryProperty"));
    }

    // call the class installer
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devInfoSet, &devInfoData))
    {
        GetLastErrorAndThrow(TEXT("SetupDiCallClassInstaller"));
    }

    class LibraryHolder
    {
    public:
        LibraryHolder(HMODULE library)
            : m_library(library)
        {}

        ~LibraryHolder()
        {
            ignore = FreeLibrary(m_library);
        }
    private:
        HMODULE m_library = nullptr;
    };

    HMODULE hLibNewdev = LoadLibrary(TEXT("newdev.dll"));
    if (!hLibNewdev)
    {
        GetLastErrorAndThrow(TEXT("LoadLibrary for \"newdev.dll\""));
    }
    LibraryHolder scoped_hLibNewdev(hLibNewdev);

    BOOL bReboot = false;
    updateDriverFN pFunc = (updateDriverFN)GetProcAddress(hLibNewdev, PROPER_FUNCTION("UpdateDriverForPlugAndPlayDevices"));
    if (!pFunc)
    {
        GetLastErrorAndThrow(TEXT("GetProcAddress for " PROPER_FUNCTION("UpdateDriverForPlugAndPlayDevices")));
    }

#if !DRY_RUN
    if (!pFunc(NULL, HWID.data(), inf.c_str(), INSTALLFLAG_FORCE, &bReboot))
    {
        GetLastErrorAndThrow(TEXT(PROPER_FUNCTION("UpdateDriverForPlugAndPlayDevices")));
    }
#endif

    rebootRequired = bReboot;
    return true;
}

bool SetIDDRegisterKeys(void)
{
    DWORD sub_key_count=0;   // number of subkeys

    HKEY key_handle;
    LONG ret_status = OpenKeyAndEnumerateInfo(
        HKEY_LOCAL_MACHINE,
        tstring(INDIRECT_DISPLAY_SUPPORT_KEY_PATH),
        &key_handle,
        &sub_key_count,
        NULL
    );

    if (ret_status != ERROR_SUCCESS) {
        return false;
    }

    // Enumerate the subkeys, until RegEnumKeyEx() fails.
    if(!sub_key_count) {
        tcout << FormatOutput(L"WARNING: RegQueryInfoKey(%s) reported no sub-keys.", INDIRECT_DISPLAY_SUPPORT_KEY_PATH) << endl;
        RegCloseKey(key_handle);
        return false;
    }

    TCHAR sub_key_name[MAX_KEY_LENGTH]; // buffer for subkey name
    DWORD sub_key_name_len;             // size of name string

    for(int sub_key_index = 0; sub_key_index < sub_key_count; sub_key_index++) {
        sub_key_name_len = MAX_KEY_LENGTH;
        ret_status = RegEnumKeyExW(
            key_handle,           // Handle to an open/predefined key
            sub_key_index,        // Index of the subkey to retrieve.
            (LPWSTR)sub_key_name, // buffer that receives the name of the subkey
            &sub_key_name_len,    // size of the buffer specified by the sub_key_name
            NULL,                 // Reserved; must be NULL
            NULL,                 // buffer that receives the class string
                                  // of the enumerated subkey
            NULL,                 // size of the buffer specified by the previous parameter
            NULL                  // variable that receives the time at which
                                  // the enumerated subkey was last written
        );

        if(ret_status != ERROR_SUCCESS) {
            continue;
        }

        DWORD value_count=0;

        HKEY idd_key_handle;
        ret_status = OpenKeyAndEnumerateInfo(
            key_handle,
            tstring(sub_key_name),
            &idd_key_handle,
            NULL,
            &value_count
        );

        if (ret_status != ERROR_SUCCESS) {
            continue;
        }

        bool is_idd_display = false;
        bool is_flex_adapter = false;

        for(int value_number = 0; value_number < value_count; value_number++) {
            TCHAR value[MAX_VALUE_NAME];
            DWORD value_len = MAX_VALUE_NAME;
            TCHAR value_data[255];
            DWORD value_data_len = sizeof(value_data);
            DWORD value_data_type;
            ret_status = RegEnumValue(
                idd_key_handle,     // Handle to an open key
                value_number,       // Index of value
                (LPWSTR)value,      // Value name
                &value_len,         // Buffer for value name
                NULL,               // Reserved
                &value_data_type,   // Value type
                (LPBYTE)value_data, // Value data
                &value_data_len     // Buffer for value data
            );

            if(ret_status == ERROR_SUCCESS) {
                if (StringToLowerCase(tstring(value)).compare(tstring(L"driverdesc")) == 0) {
                    if (StringToLowerCase(tstring(value_data)).compare(tstring(L"intel iddsampledriver device")) == 0) {
                        tcout << FormatOutput(L"IDD Display found: %s", sub_key_name) << endl;
                        is_idd_display = true;
                    }
                }
                if (StringToLowerCase(tstring(value)).compare(tstring(L"matchingdeviceid")) == 0) {
                    if (StringToLowerCase(tstring(value_data)).find(tstring(L"ven_8086&dev_56c1")) != std::string::npos) {
                        tcout << FormatOutput(L"Flex GPU Adapter found: %s", sub_key_name) << endl;
                        is_flex_adapter = true;
                    } else if (StringToLowerCase(tstring(value_data)).find(tstring(L"ven_8086&dev_56c0")) != std::string::npos) {
                        tcout << FormatOutput(L"Flex GPU Adapter found: %s", sub_key_name) << endl;
                        is_flex_adapter = true;
                    }
                }
            }
        }

        if (is_idd_display) {
            // Nothing to set for IDD displays currently.
        }

        if (is_flex_adapter) {
            DWORD value_data_type = REG_DWORD;
            DWORD value_data = 0x1;
            DWORD value_data_len = sizeof(value_data);
            ret_status = RegSetKeyValueW(
                idd_key_handle,            // Key
                NULL,                      // Subkey
                L"IndirectDisplaySupport", // Name of the value to set
                value_data_type,           // Value type
                (LPBYTE)&value_data,       // Value to set
                value_data_len             // Length of the value
            );

            if(ret_status != ERROR_SUCCESS) {
                tcout << FormatOutputWithOffset(1, L"Failed to set IndirectDisplaySupport for GPU adapter: %s", sub_key_name) << endl;
            } else {
                tcout << FormatOutputWithOffset(1, L"Successfully set IndirectDisplaySupport for GPU adapter: %s", sub_key_name) << endl;
            }
        }

        RegCloseKey(idd_key_handle);
    }

    RegCloseKey(key_handle);
    return true;
}
