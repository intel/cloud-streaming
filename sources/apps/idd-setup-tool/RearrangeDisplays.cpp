// Copyright (C) 2022-2023 Intel Corporationation
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

#include "RearrangeDisplays.h"
#include <unordered_map>

using namespace std;

#define DEBUG_PRINT 0

static std::unordered_map<tstring, uint32_t> PatternMatchCounts {};
static std::unordered_map<tstring, std::vector<tstring>> DeviceNamesForIndexedPatterns {};

static bool CheckAndPrintDisplayChangeStatus(LONG sts)
{
    switch (sts)
    {
    case DISP_CHANGE_SUCCESSFUL:
        tcout << FormatOutput(L"Display Change Status (PASS): DISP_CHANGE_SUCCESSFUL (%d)", DISP_CHANGE_SUCCESSFUL) << endl;
        return true;

    case DISP_CHANGE_RESTART:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_RESTART (%d)", DISP_CHANGE_RESTART) << endl;
        break;

    case DISP_CHANGE_FAILED:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_FAILED (%d)", DISP_CHANGE_FAILED) << endl;
        break;

    case DISP_CHANGE_BADMODE:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_BADMODE (%d)", DISP_CHANGE_BADMODE) << endl;
        break;

    case DISP_CHANGE_NOTUPDATED:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_NOTUPDATED (%d)", DISP_CHANGE_NOTUPDATED) << endl;
        break;

    case DISP_CHANGE_BADFLAGS:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_BADFLAGS (%d)", DISP_CHANGE_BADFLAGS) << endl;
        break;

    case DISP_CHANGE_BADPARAM:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_BADPARAM (%d)", DISP_CHANGE_BADPARAM) << endl;
        break;

    case DISP_CHANGE_BADDUALVIEW:
        tcout << FormatOutput(L"Display Change Status (FAIL): DISP_CHANGE_BADDUALVIEW (%d)", DISP_CHANGE_BADDUALVIEW) << endl;
        break;
    }

    return false;
}

static void PrintStateFlags(DWORD flags)
{
    if (flags)
    {
        tcout << FormatOutputWithOffset(1, L"StateFlags are:") << endl;

        if (flags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_ATTACHED_TO_DESKTOP") << endl;

        if (flags & DISPLAY_DEVICE_MULTI_DRIVER)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_MULTI_DRIVER") << endl;

        if (flags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_PRIMARY_DEVICE") << endl;

        if (flags & DISPLAY_DEVICE_MIRRORING_DRIVER)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_MIRRORING_DRIVER") << endl;

        if (flags & DISPLAY_DEVICE_VGA_COMPATIBLE)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_VGA_COMPATIBLE") << endl;

        if (flags & DISPLAY_DEVICE_REMOVABLE)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_REMOVABLE") << endl;

        if (flags & DISPLAY_DEVICE_ACC_DRIVER)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_ACC_DRIVER") << endl;

        if (flags & DISPLAY_DEVICE_MODESPRUNED)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_MODESPRUNED") << endl;

        if (flags & DISPLAY_DEVICE_RDPUDD)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_RDPUDD") << endl;

        if (flags & DISPLAY_DEVICE_REMOTE)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_REMOTE") << endl;

        if (flags & DISPLAY_DEVICE_DISCONNECT)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_DISCONNECT") << endl;

        if (flags & DISPLAY_DEVICE_TS_COMPATIBLE)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_TS_COMPATIBLE") << endl;

        if (flags & DISPLAY_DEVICE_UNSAFE_MODES_ON)
            tcout << FormatOutputWithOffset(2, L"DISPLAY_DEVICE_UNSAFE_MODES_ON") << endl;
    }
}

static tstring EraseFromStartOfString(tstring main_str, const tstring to_erase)
{
    // Search for the substring in string
    size_t pos = StringToLowerCase(main_str).find(StringToLowerCase(to_erase));

    if (pos != 0) {
        // If not at start then dont erase.
        return main_str;
    }

    if (pos != tstring::npos) {
        // If found then erase it from string
        main_str.erase(pos, to_erase.length());
    }


    return main_str;
}

static tstring LookupDeviceIDForDisplay(tstring device_key)
{
    DWORD sub_key_count=0;   // number of subkeys
    DWORD sub_val_count=0;   // number of subvalue

    // String we are searching for is always in HKEY_LOCAL_MACHINE. Likewise the format of the passed in device_key must be adjusted.
    device_key = EraseFromStartOfString(device_key, tstring(TEXT("\\REGISTRY\\MACHINE\\")));

    HKEY key_handle;
    LONG ret_status = OpenKeyAndEnumerateInfo(
        HKEY_LOCAL_MACHINE,
        device_key,
        &key_handle,
        &sub_key_count,
        &sub_val_count
    );

    if (ret_status != ERROR_SUCCESS) {
        return tstring(TEXT(""));
    }

    tstring device_id;

    for(int value_number = 0; value_number < sub_val_count; value_number++) {
        TCHAR value[MAX_VALUE_NAME];
        DWORD value_len = MAX_VALUE_NAME;
        TCHAR value_data[255];
        DWORD value_data_len = sizeof(value_data);
        DWORD value_data_type;
        ret_status = RegEnumValue(
            key_handle,         // Handle to an open key
            value_number,       // Index of value
            (LPWSTR)value,      // Value name
            &value_len,         // Buffer for value name
            NULL,               // Reserved
            &value_data_type,   // Value type
            (LPBYTE)value_data, // Value data
            &value_data_len     // Buffer for value data
        );

        if(ret_status == ERROR_SUCCESS) {
            if (StringToLowerCase(tstring(value)).compare(tstring(L"matchingdeviceid")) == 0) {
                device_id = tstring(value_data);
            }
        }
    }

    RegCloseKey(key_handle);

    return device_id;
}

static void CheckEnableDisablePatternMatches(tstring device_string, tstring device_id, tstring device_name, bool verbose)
{
    if (verbose) {
        tcout << FormatOutputWithOffset(1, L"Display can be enabled and disabled with patterns:") << endl;
    }
    for (EnableDisablePatternStruct& pattern : SupportedEnableDisablePatterns) {
        for (tstring sub_pattern : pattern.DisplaysToMatch) {
            bool matches_this_pattern = false;
            if (CheckIfStringContainsPattern(device_string, sub_pattern, true) != pattern.IsAnInvertedTarget) {
                matches_this_pattern = true;
            }
            if (pattern.PatternType == ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION &&
                CheckIfStringContainsPattern(device_string, sub_pattern, true) != pattern.IsAnInvertedTarget) {
                matches_this_pattern = true;
            } else if (pattern.PatternType == ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID &&
                CheckIfStringContainsPattern(device_id, sub_pattern, true) != pattern.IsAnInvertedTarget) {
                matches_this_pattern = true;
            }
            if (matches_this_pattern) {
                if (verbose) {
                    tcout << FormatOutputWithOffset(2, L"%s", pattern.Abbreviation.c_str()) << endl;
                }
                if (PatternMatchCounts.find(pattern.Abbreviation) == PatternMatchCounts.end()) {
                    PatternMatchCounts[pattern.Abbreviation] = 1;
                    DeviceNamesForIndexedPatterns[pattern.Abbreviation] = std::vector<tstring>();
                } else {
                    PatternMatchCounts[pattern.Abbreviation]++;
                }
                DeviceNamesForIndexedPatterns[pattern.Abbreviation].push_back(device_name);
                if (verbose) {
                    tcout << FormatOutputWithOffset(2, L"%s%u", pattern.Abbreviation.c_str(), PatternMatchCounts[pattern.Abbreviation]) << endl;
                }
                break;
            }
        }
    }
}

static void PrintDisplayDevice(DISPLAY_DEVICE display_device, tstring device_id, DWORD idev_num)
{
    tcout << FormatOutputWithOffset(1, L"Device number: %lu", idev_num);
    if (display_device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        tcout << L" <------ Primary Device";
    tcout << endl;

    tcout << FormatOutputWithOffset(2, L"Device Name       : %s", display_device.DeviceName) << endl;
    tcout << FormatOutputWithOffset(2, L"Device String     : %s", display_device.DeviceString) << endl;
    tcout << FormatOutputWithOffset(2, L"State Flags       : %lu", display_device.StateFlags) << endl;
    tcout << FormatOutputWithOffset(2, L"Device ID         : %s", display_device.DeviceID) << endl;
    tcout << FormatOutputWithOffset(2, L"Device Key        : %s", display_device.DeviceKey) << endl;
    tcout << FormatOutputWithOffset(2, L"Matching Device ID: %s", device_id.c_str()) << endl;
    IncIndentation();
    PrintStateFlags(display_device.StateFlags);
    DecIndentation();
    IncIndentation();
    CheckEnableDisablePatternMatches(display_device.DeviceString, device_id, display_device.DeviceName, true);
    DecIndentation();
}

static void PrintPosition(DEVMODE dev_mode)
{
    tcout << FormatOutputWithOffset(1, L"Position:") << endl;
    tcout << FormatOutputWithOffset(2, L"x     : %ld", dev_mode.dmPosition.x) << endl;
    tcout << FormatOutputWithOffset(2, L"y     : %ld", dev_mode.dmPosition.y) << endl;
    tcout << FormatOutputWithOffset(2, L"width : %lu", dev_mode.dmPelsWidth) << endl;
    tcout << FormatOutputWithOffset(2, L"height: %lu", dev_mode.dmPelsHeight) << endl;
}

// This function actually queries topology of screen and returns it back. No changes to displays arrangement happen here
list_of_settings QueryActiveSettings(bool verbose, bool extract_only_attached)
{
    list_of_settings active_settings;

    PatternMatchCounts.clear();
    DeviceNamesForIndexedPatterns.clear();

    if (verbose)
        tcout << FormatOutput(L"Starting enumeration...") << endl;

    // MSFT way to run through all displays on system is to query until it fails on some index
    for (DWORD idev_num = 0;; ++idev_num)
    {
        DISPLAY_DEVICE display_device = {};
        display_device.cb = sizeof(DISPLAY_DEVICE);

        // Get info for display indexed as idev_num, this usually matches to digits on screen during "Detect display"
        // Here we obtain some basic info like Name of the adapter, State (attached/detached, main display), reg. key location
        if (!EnumDisplayDevices(nullptr, idev_num, &display_device, EDD_GET_DEVICE_INTERFACE_NAME)) {
            tcout << FormatOutputWithOffset(1, L"Enumerated %lu displays", idev_num) << endl;
            break;
        }

        tstring device_id = LookupDeviceIDForDisplay(tstring(display_device.DeviceKey));

        if (verbose) {
            PrintDisplayDevice(display_device, device_id, idev_num);
        } else {
            CheckEnableDisablePatternMatches(display_device.DeviceString, device_id, display_device.DeviceName, false);
        }

        DEVMODE current_settings = {};
        current_settings.dmSize = sizeof(DEVMODE);

        // This will error out if display is not attached.
        if (display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            if (verbose) {
                tcout << FormatOutput(L"Getting current display settings...") << endl;
            }
            // Now retrieving settings for selected display
            // Gives more info about display as physical device: offset from main display, width/height in pxls, bpp, etc.
            auto ret = EnumDisplaySettings(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &current_settings);

            if (!ret) {
                if (verbose) {
                    tcout << FormatOutputWithOffset(1, L"ERROR: EnumDisplaySettings (Current Settings) failed with status %lu: %s", GetLastError(), GetLastErrorString().c_str()) << endl;
                }
                continue;
            }

            if (verbose) {
                PrintPosition(current_settings);
            }
        } else if (extract_only_attached == false) {
            auto ret = EnumDisplaySettings(display_device.DeviceName, ENUM_REGISTRY_SETTINGS, &current_settings);
            if (!ret) {
                if (verbose) {
                    tcout << FormatOutputWithOffset(1, L"ERROR: EnumDisplaySettings (Registry Settings) failed with status %lu: %s", GetLastError(), GetLastErrorString().c_str()) << endl;
                }
                continue;
            }
        }

        // Some of the displays are not corresponding to physically present displays.
        if (extract_only_attached == false || display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            active_settings.emplace_back(make_pair(display_device, current_settings));
        }
    }

    if (verbose)
        tcout << FormatOutput(L"End of enumeration...") << endl;

    return active_settings;
}

// Here the magic happens. Displays are sorted in correct order in terms of their ids.
// First display in a row made primary
int RearrangeDisplays(list_of_settings active_settings)
{
    if (active_settings.empty())
        return -1;

    // Here we starting iteration over adapters to put them in correct order as screens
    // (we store them in order as got from QueryActiveSettings, so they are already sorted by ids,
    // but may not be sorted in same way in terms of display coordinates, we fixing it).

    auto it_prev = begin(active_settings);

    // Making first adapter to be our primary display
    DEVMODE* p_dev_mode = &it_prev->second;
    // Primary adapter has coordinates 0,0 always
    p_dev_mode->dmPosition.x = 0;
    // Changing y is not necessary, we consider only horizontal layout, so y is 0 for all displays
    p_dev_mode->dmPosition.y = 0;

#if DEBUG_PRINT
    int32_t tmp_idx = 0;
    PrintDisplayDevice(it_prev->first, tmp_idx++);
    PrintPosition(*p_dev_mode);
#endif

    // This API operates as state machine, first we push new config for each display to Registry. At the end apply them all
#if !DRY_RUN
    auto ret = ChangeDisplaySettingsEx(it_prev->first.DeviceName, p_dev_mode, NULL, CDS_SET_PRIMARY | CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
    if (!CheckAndPrintDisplayChangeStatus(ret))
        return -1;
#endif

    // Here we iterate over all adapters setting their offset carefully to have them all stacked horizontally.
    //
    // So briefly: Display[i].x_coord = Display[i-1].x_coord + Display[i-1].width

    for (auto it_curr = next(it_prev); it_curr != end(active_settings); ++it_curr, ++it_prev)
    {
        p_dev_mode = &it_curr->second;

        p_dev_mode->dmPosition.x = it_prev->second.dmPosition.x + it_prev->second.dmPelsWidth;
        //NOTE: assumed pure horizontal layout, so dev_mode.dmPosition.y not touched

#if DEBUG_PRINT
        PrintDisplayDevice(it_curr->first, tmp_idx++);
        PrintPosition(*p_dev_mode);
#endif

        // Pushing new state for current display
#if !DRY_RUN
        auto ret = ChangeDisplaySettingsEx(it_curr->first.DeviceName, p_dev_mode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
        if (!CheckAndPrintDisplayChangeStatus(ret))
            return -1;
#endif
    }

    // This function just applies all pushed changes, making screen blink for ~1s
    //
    // As result you should see stacked displays in correct order and MSFT Basic will be last one
#if !DRY_RUN
    ret = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
    if (!CheckAndPrintDisplayChangeStatus(ret))
        return -1;
#endif

    return 0;
}

int EnableDisableDisplay(list_of_settings active_settings, display_target_info_t &target_info)
{
    if (active_settings.empty()) {
        return -1;
    }

    for (auto it_adapter = begin(active_settings); it_adapter != end(active_settings); it_adapter++) {
        tstring device_id = LookupDeviceIDForDisplay(tstring(it_adapter->first.DeviceKey));
        bool is_match = false;
        if (target_info.pattern_type == ENABLE_DISABLE_PATTERN_TYPES::DESCRIPTION &&
            CheckIfStringContainsPattern(it_adapter->first.DeviceString, target_info.pattern, true) != target_info.is_an_inverted_target) {
            is_match = true;
        } else if (target_info.pattern_type == ENABLE_DISABLE_PATTERN_TYPES::VENDOR_AND_DEVICE_ID &&
            CheckIfStringContainsPattern(device_id, target_info.pattern, true) != target_info.is_an_inverted_target) {
            is_match = true;
        }

        if (is_match) {
            if (target_info.target_device_name.length() > 0 && target_info.target_device_name.find(it_adapter->first.DeviceName) != 0) {
                // We aren't the specific display requested. Skip this display.
                continue;
            }

            DEVMODE* p_dev_mode;
            p_dev_mode = &it_adapter->second;
            p_dev_mode->dmPelsWidth = target_info.default_width;
            p_dev_mode->dmPelsHeight = target_info.default_height;

            auto ret = ChangeDisplaySettingsEx(it_adapter->first.DeviceName, p_dev_mode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
            if (!CheckAndPrintDisplayChangeStatus(ret)) {
                return -1;
            }
        }
    }

    // This function just applies all pushed changes, making screen blink for ~1s
    //
    // As result you should see stacked displays in correct order and MSFT Basic will be last one
#if !DRY_RUN
    auto ret = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
    if (!CheckAndPrintDisplayChangeStatus(ret)) {
        return -1;
    }
#endif

    return 0;
}

int EnableDisableDisplayManager(tstring Pattern, list_of_settings active_settings, bool enable)
{
    Pattern = StringToLowerCase(Pattern);

    tstring temp_string = TEXT("disable");
    if (enable) {
        temp_string = TEXT("enable");
    }

    int status = 1;

    for (EnableDisablePatternStruct& possible_pattern_match : SupportedEnableDisablePatterns) {
        bool is_a_full_match = false;
        if (Pattern.find(possible_pattern_match.Abbreviation) == 0) {
            is_a_full_match = true;
            tstring target_device_name = TEXT("");
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
                        L"The specified display to %s '%s' matches a valid display pattern '%s' but index '%d' is out of range (max of '%d', min of '1').",
                        temp_string.c_str(),
                        Pattern.c_str(),
                        possible_pattern_match.Abbreviation.c_str(),
                        target_index,
                        PatternMatchCounts[possible_pattern_match.Abbreviation]) << endl;
                        is_a_full_match = false;
                    } else {
                        target_device_name = DeviceNamesForIndexedPatterns[possible_pattern_match.Abbreviation][target_index - 1];
                    }
                } else {
                    is_a_full_match = false;
                }
            }

            if (is_a_full_match) {
                for (tstring sub_pattern : possible_pattern_match.DisplaysToMatch) {
                    display_target_info_t target_info;
                    target_info.pattern = sub_pattern;
                    target_info.is_an_inverted_target = possible_pattern_match.IsAnInvertedTarget;
                    target_info.target_device_name = target_device_name;
                    target_info.pattern_type = possible_pattern_match.PatternType;
                    if (enable) {
                        target_info.default_width = 1920;
                        target_info.default_height = 1080;
                        if (DisplayToResolutionMap.find(sub_pattern) != DisplayToResolutionMap.end() &&
                            DisplayToResolutionMap[sub_pattern].PatternType == possible_pattern_match.PatternType) {
                            target_info.default_width = DisplayToResolutionMap[sub_pattern].width;
                            target_info.default_height = DisplayToResolutionMap[sub_pattern].height;
                        }
                    } else {
                        target_info.default_width = 0;
                        target_info.default_height = 0;
                    }
                    status &= EnableDisableDisplay(active_settings, target_info);
                }
                return status;
            }
        }
    }

    tcout << FormatOutputWithOffset(1, 
        L"The specified display to %s '%s' matched no display patterns supported by this tool.", temp_string.c_str(), Pattern.c_str()) << endl;
    return status;
}

int DisableDisplay(tstring Pattern, list_of_settings active_settings)
{
    return EnableDisableDisplayManager(Pattern, active_settings, false);
}

int EnableDisplay(tstring Pattern, list_of_settings active_settings)
{
    return EnableDisableDisplayManager(Pattern, active_settings, true);
}
