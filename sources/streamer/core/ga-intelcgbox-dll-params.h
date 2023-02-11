// Copyright (C) 2022 Intel Corporation
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
#include "io-common.h"
typedef struct
{
    PVOID   Kernel32Address;
    int     Kernel32Size;
    int     load_library_ex_w_offset;
    int     create_file_offset;
    int     write_file_offset;
    int     close_handle_offset;
    int     get_current_process_id_offset;
    int     device_io_control_offset;

    WCHAR    HookDllName[CG_MAX_FILE_NAME];
    WCHAR    fileName[CG_MAX_FILE_NAME];
    CHAR     GameName[CG_MAX_FILE_NAME];
}   INJECT_DLL_PARAMS, *PINJECT_DLL_PARAMS;
