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

#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <Wingdi.h>
#include <Winuser.h>
#include <SetupAPI.h>
#include <stringapiset.h>

#define DRY_RUN 0// Debug regime, no changes really applied
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

#define INDIRECT_DISPLAY_SUPPORT_KEY_PATH L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}"
// "HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}"

std::filesystem::path getExePath();

static inline std::filesystem::path GetDefaultIddPath(void) {
    return getExePath().replace_filename("idd");
}

static inline bool isIddOk(const std::filesystem::path& idd) {
    bool check = true;
    check &= std::filesystem::is_regular_file(idd / "IddSampleDriver.inf");
    check &= std::filesystem::is_regular_file(idd / "IddSampleDriver.dll");
    check &= std::filesystem::is_regular_file(idd / "iddsampledriver.cat");

    return check;
}

// Wide / regular chars supported in entire code

static std::wstring s2ws(const std::string& str)
{
    int32_t size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}
static std::string ws2s(const std::wstring& wstr)
{
    int32_t size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, 0, 0);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, 0, 0);
    return str;
}
#if defined(UNICODE) || defined(_UNICODE)
#define tcout std::wcout
#define PROPER_FUNCTION(FUNCTION) FUNCTION##"W"
#define get_tstring_from_chars(CHAR_ARRAY) s2ws(std::string(CHAR_ARRAY))
#define get_tstring_from_wchars(CHAR_ARRAY) std::wstring(CHAR_ARRAY)
#define get_int_from_string(STRING) std::stoi(STRING)
#define get_int_from_wstring(STRING) std::stoi(ws2s(STRING))
#define get_int_from_tstring(STRING) get_int_from_wstring(STRING)
#else
#define tcout std::cout
#define PROPER_FUNCTION(FUNCTION) FUNCTION##"A"
#define get_tstring_from_chars(CHAR_ARRAY) std::string(CHAR_ARRAY)
#define get_tstring_from_wchars(CHAR_ARRAY) ws2s(std::wstring(CHAR_ARRAY))
#define get_int_from_string(STRING) std::stoi(STRING)
#define get_int_from_wstring(STRING) std::stoi(ws2s(STRING))
#define get_int_from_tstring(STRING) get_int_from_string(STRING)
#endif

using tstring = std::basic_string<TCHAR>;
using tstringstream = std::basic_stringstream<TCHAR>;

enum ENABLE_DISABLE_PATTERN_TYPES {
    DESCRIPTION = 0,
    VENDOR_AND_DEVICE_ID
};

typedef struct ENABLE_DISABLE_PATTERN {
    tstring Abbreviation = TEXT("");
    std::list<tstring> DisplaysToMatch = {};
    std::list<tstring> AdaptersToMatch = {};
    bool IsAnInvertedTarget = false;
    ENABLE_DISABLE_PATTERN_TYPES PatternType;
} EnableDisablePatternStruct;

static std::list<EnableDisablePatternStruct> SupportedEnableDisablePatterns = {
    {TEXT("flex")    , {}                                        , {TEXT("VEN_8086&DEV_56C0"),
                                                                    TEXT("VEN_8086&DEV_56C1")}               , false, ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID},
    {TEXT("non-flex"), {TEXT("VEN_8086&DEV_56C1"),
                        TEXT("VEN_8086&DEV_56C1")}               , {}                                        , true , ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID},
    {TEXT("idd")     , {TEXT("Intel IddSampleDriver Device")}    , {TEXT("Intel IddSampleDriver Device")}    , false, ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION},
    {TEXT("non-idd") , {TEXT("Intel IddSampleDriver Device")}    , {}                                        , true , ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION},
    {TEXT("msft")    , {TEXT("Microsoft Basic Display")}         , {TEXT("Microsoft Basic Display Adapter")} , false, ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION},
    {TEXT("virtio")  , {TEXT("Red Hat VirtIO GPU DOD controller"),
                        TEXT("Red Hat QXL controller")          }, {}                                        , false, ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION}
};

typedef struct RESOLUTION {
    int32_t width = 0;
    int32_t height = 0;
    ENABLE_DISABLE_PATTERN_TYPES PatternType;
} ResolutionStruct;

static std::unordered_map<tstring, ResolutionStruct> DisplayToResolutionMap = {
    {TEXT("VEN_8086&DEV_56C0")                , {1920, 1080, ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID}},
    {TEXT("VEN_8086&DEV_56C1")                , {1920, 1080, ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID}},
    {TEXT("Intel IddSampleDriver Device")     , {1920, 1080, ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION}},
    {TEXT("Microsoft Basic Display")          , {1024, 768 , ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION}},
    {TEXT("Red Hat VirtIO GPU DOD controller"), {1280, 1024, ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION}},
    {TEXT("Red Hat QXL controller")           , {1024, 768 , ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION}},
};

class ClassDevsScopedStorage
{
public:
    ClassDevsScopedStorage(HDEVINFO devs)
        : m_devs(devs)
    {}

    ~ClassDevsScopedStorage()
    {
        if (m_devs != INVALID_HANDLE_VALUE)
        {
            // (unclear from docs whether SetupDiDeleteDeviceInfo must also be called explicitly, or will be called inside here)
            std::ignore = SetupDiDestroyDeviceInfoList(m_devs);
        }
    }

private:
    HDEVINFO m_devs = INVALID_HANDLE_VALUE;
};

tstring StringToLowerCase(tstring str);
bool CheckIfStringContainsPattern(tstring target, tstring pattern, bool ignore_case = true);
std::vector<tstring> SplitStringOnDelimiter(tstring Content, tstring Delimeter);
std::string GetLastErrorString();
void GetLastErrorAndThrow(tstring function_name);
bool RunSystemCommand(const tstring& cmdline, tstring& result);
tstring FormatString(const TCHAR *format_string, ...);
tstring FormatOutput(const TCHAR *format_string, ...);
tstring FormatOutputWithOffset(uint32_t offset_amount, const TCHAR *format_string, ...);
void IncIndentation();
void DecIndentation();
LONG OpenKeyAndEnumerateInfo(HKEY base_key, tstring target_key, HKEY* key_handle, DWORD* sub_key_count, DWORD* sub_value_count);
