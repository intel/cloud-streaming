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

#include "IddSetupToolCommandParser.h"
#include "Utility.h"

#include <initguid.h>
#include "Guids.h"

std::unique_ptr<IddSetupToolCommandParser> g_params;

std::filesystem::path getExePath()
{
    tstring exe(MAX_PATH, 0);
    auto path_len = GetModuleFileName(nullptr, exe.data(), exe.size());
    if (path_len == 0) {
        tcout << FormatOutput(L"Error: GetModuleFileName() failed") << std::endl;
        return std::filesystem::path();
    }
    return std::filesystem::path(exe);
}

tstring StringToLowerCase(tstring TargetString)
{
    std::transform(TargetString.begin(), TargetString.end(), TargetString.begin(),
        [](TCHAR SubChar){ return std::tolower(SubChar); });
    return TargetString;
}

// Compare strings, ignore case by default.
bool CheckIfStringContainsPattern(tstring target, tstring pattern, bool ignore_case)
{
    if (ignore_case) {
        target = StringToLowerCase(target);
        pattern = StringToLowerCase(pattern);
    }
    // Check if target by comparing strings
    return tstring(target).find(pattern) != tstring::npos;
};

std::vector<tstring> SplitStringOnDelimiter(tstring Content, tstring Delimeter)
{
    std::vector<tstring> Result;
    auto PreviousPosition = Content.begin();
    auto NextPosition = std::search(PreviousPosition, Content.end(),
                               Delimeter.begin(), Delimeter.end());
    while (NextPosition != Content.end())
    {
        Result.emplace_back(PreviousPosition, NextPosition);
        PreviousPosition = NextPosition + Delimeter.size();
        NextPosition = std::search(PreviousPosition, Content.end(),
                               Delimeter.begin(), Delimeter.end());
    }

    if (PreviousPosition != Content.end())
    {
        Result.emplace_back(PreviousPosition, Content.end());
    }
    return Result;
}

std::string GetLastErrorString()
{
    using namespace std;

    //Get the error message ID, if any.
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0)
    {
        return string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a string.
    string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    // Remove trailing \r\n
    message.erase(remove(begin(message), end(message), '\n'), cend(message));
    message.erase(remove(begin(message), end(message), '\r'), cend(message));

    return message;
}

void GetLastErrorAndThrow(tstring function_name)
{
    tstringstream ss;
    ss << "ERROR: " << function_name << " failed with status " << std::hex << GetLastError() << " (" << s2ws(GetLastErrorString()) << ")\n";
    throw ss.str();
}

/**
 * Create child process executes system command and return result in string
 *
 * @param cmd [in] The command to execute
 * @param result [out] The return result in string
 *
 * @return Returns true if success, false otherwise
 */
bool RunSystemCommand(const tstring& cmdline, tstring& result) {
    result.clear();

    bool ResultStatus = true;

    HANDLE h_child_stdin_read = nullptr, h_child_stdin_write = nullptr;
    HANDLE h_child_stdout_read = nullptr, h_child_stdout_write = nullptr;
    SECURITY_ATTRIBUTES sa = {};

    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    /*
     * +---------------------+                +---------------------+
     * |   Parent Process    |                |   Child Process     |
     * +---------------------+                +---------------------+
     * |                     |                |                     |
     * | h_child_stdin_write | -------------> | h_child_stdin_Read  |
     * |                     |                |                     |
     * | h_child_stdout_read | <------------- | h_child_stdout_write|
     * |                     |                |                     |
     * +---------------------+                +---------------------+
     *
     */

    // Create child stdin read/write pipe
    if (CreatePipe(&h_child_stdin_read, &h_child_stdin_write, &sa, 0) == FALSE)
        return false;

    // Create child stdout read/write pipe
    if (CreatePipe(&h_child_stdout_read, &h_child_stdout_write, &sa, 0) == FALSE) {
        CloseHandle(h_child_stdin_read);
        CloseHandle(h_child_stdin_write);
        return false;
    }

    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};

    // Set STRTUPINFO for the spawned process
    // The dwFlags member tells CreateProcess how to make the process
    //   STARTF_USESTDHANDLES: validates the hStd* members.
    //   STARTF_USESHOWWINDOW: validates the wShowWindow member
    GetStartupInfo(&si);

    si.dwFlags      = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow  = SW_HIDE;  // Silent command window
    si.hStdOutput   = h_child_stdout_write;
    si.hStdError    = h_child_stdout_write;
    si.hStdInput    = h_child_stdin_read;

    // spawn the child process to executes the command
    if (CreateProcess(nullptr, (LPWSTR)cmdline.c_str(), nullptr, nullptr, TRUE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        const size_t max_buffer_size = 4096;
        unsigned long bytes_read = 0;
        unsigned long bytes_avail = 0;
        CHAR buf[max_buffer_size] = {};

        // Command executes normally quick respond.
        // Wait until child process exits just in-case.
        WaitForSingleObject(pi.hProcess, INFINITE);

        for (;;) {
            PeekNamedPipe(h_child_stdout_read, buf, sizeof(buf) - 1, &bytes_read, &bytes_avail, nullptr);
            // Check to see if there is any data ready to read from stdout
            if (bytes_read != 0) {
                if (ReadFile(h_child_stdout_read, buf, sizeof(buf) - 1, &bytes_read, nullptr)) {
                    // Convert to a wchar_t*
                    size_t buf_size = strlen(buf) + 1;
                    const size_t new_size = max_buffer_size;
                    size_t converted_chars = 0;
                    wchar_t wcstring[new_size];
                    mbstowcs_s(&converted_chars, wcstring, buf_size, buf, _TRUNCATE);
                    result += tstring(wcstring);
                    break;
                }
            }
        }
        // Close thread and process handles
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        ResultStatus = false;
    }
    // Close all pipe handles
    CloseHandle(h_child_stdin_read);
    CloseHandle(h_child_stdin_write);
    CloseHandle(h_child_stdout_read);
    CloseHandle(h_child_stdout_write);

    // Remove last newline if appeared
    if (!result.empty() && result[result.size() - 1] == '\n') {
        result.pop_back();
    }

    return ResultStatus;
}

tstring FormatString(const TCHAR *format_string, ...)
{
    // Construct the formatted source message
    TCHAR buffer[4096];
    va_list args;
    va_start(args, format_string);
    vswprintf(buffer, 4096, format_string, args);
    va_end(args);
    tstring source_message = tstring(buffer);

    return source_message;
}

tstring FormatOutput(const TCHAR *format_string, ...)
{
    // Construct the formatted source message
    TCHAR buffer[4096];
    va_list args;
    va_start(args, format_string);
    vswprintf(buffer, 4096, format_string, args);
    va_end(args);
    tstring source_message = tstring(buffer);

    // Prepare indentation to insert
    tstring indentation_string = L"";
    if (g_params != NULL)
    {
        for (int indent = 0; indent < g_params->Options.IndentationLevel; indent++)
        {
            indentation_string += L"  ";
        }
    }

    // Iterate over lines in source message and insert indentation
    int32_t start_location = 0;
    size_t end_location = source_message.find(L'\n', start_location);
    tstring destination = L"";
    while (end_location != tstring::npos)
    {
        tstring substring = source_message.substr(start_location, end_location - start_location);
        substring.erase(remove(begin(substring), end(substring), '\n'), cend(substring));
        substring.erase(remove(begin(substring), end(substring), '\r'), cend(substring));
        destination += indentation_string + substring + L"\n";
        start_location = end_location + 1;
        end_location = source_message.find(L'\n', start_location);
    }

    tstring substring = source_message.substr(start_location, source_message.length() - start_location);
    substring.erase(remove(begin(substring), end(substring), '\n'), cend(substring));
    substring.erase(remove(begin(substring), end(substring), '\r'), cend(substring));
    destination += indentation_string + substring;

    return destination;
}

tstring FormatOutputWithOffset(uint32_t offset_amount, const TCHAR *format_string, ...)
{
    // Construct the formatted source message
    TCHAR buffer[4096];
    va_list args;
    va_start(args, format_string);
    vswprintf(buffer, 4096, format_string, args);
    va_end(args);
    tstring source_message = tstring(buffer);

    // Call FormatOutput with temporary indentation
    for (int indent = 0; indent < offset_amount; indent++)
    {
        IncIndentation();
    }
    tstring formatted_message = FormatOutput(source_message.c_str());
    for (int indent = 0; indent < offset_amount; indent++)
    {
        DecIndentation();
    }

    return formatted_message;
}

void IncIndentation()
{
    if (g_params == NULL) return;
    g_params->Options.IndentationLevel++;
}

void DecIndentation()
{
    if (g_params == NULL) return;
    g_params->Options.IndentationLevel--;
}

LONG OpenKeyAndEnumerateInfo(HKEY base_key, tstring target_key, HKEY* key_handle, DWORD* sub_key_count, DWORD* sub_value_count)
{
    LONG ret_status = RegOpenKeyExW(
        base_key,           // Key
        target_key.c_str(), // Subkey
        0,                  // Reserved
        KEY_ALL_ACCESS,     // desired access rights to the key
        key_handle          // variable that receives a handle to the opened key
    );

    if(ret_status != ERROR_SUCCESS) {
        return ret_status;
    }

    // Get the class name and the value count.
    ret_status = RegQueryInfoKeyW(
        *key_handle,     // key handle
        NULL,            // buffer for class name
        NULL,            // size of class string
        NULL,            // reserved
        sub_key_count,   // number of subkeys
        NULL,            // longest subkey size
        NULL,            // longest class string
        sub_value_count, // number of values for this key
        NULL,            // longest value name
        NULL,            // longest value data
        NULL,            // security descriptor
        NULL             // last write time
    );

    if(ret_status != ERROR_SUCCESS) {
        RegCloseKey(*key_handle);
        return ret_status;
    }

    return ERROR_SUCCESS;
}
