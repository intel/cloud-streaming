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

#ifdef _WIN32

#define CG_MAX_PATH 260
#define CG_MAX_FILE_NAME 260

#ifndef MAGIC_IO_CODE
#define MAGIC_IO_CODE 0x55AA55AA
#endif

#define CG_BOX_CG_CONFIG_INFO_FUNCTION_CODE   0x810
#define CG_BOX_SET_TARGET_PID_FUNCTION_CODE   0x811
#define CG_BOX_QUERY_TARGET_PID_FUNCTION_CODE 0x812

#define CG_BOX_IO_CTL_CG_CONFIG_INFO   CTL_CODE(FILE_DEVICE_UNKNOWN, CG_BOX_CG_CONFIG_INFO_FUNCTION_CODE  , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CG_BOX_IO_CTL_SET_TARGET_PID   CTL_CODE(FILE_DEVICE_UNKNOWN, CG_BOX_SET_TARGET_PID_FUNCTION_CODE  , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CG_BOX_IO_CTL_QUERY_TARGET_PID CTL_CODE(FILE_DEVICE_UNKNOWN, CG_BOX_QUERY_TARGET_PID_FUNCTION_CODE, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef UNICODE
#define CG_BOX_DEVICE_OBJECT_NAME    L"\\Device\\CgBoxDevice"
#define CG_BOX_DEVICE_LINK_NAME      L"\\DosDevices\\CgBoxDevice"
#define CG_BOX_DEVICE_LINK_NAME_USER L"\\\\.\\CgBoxDevice"
#else
#define CG_BOX_DEVICE_OBJECT_NAME     "\\Device\\CgBoxDevice"
#define CG_BOX_DEVICE_LINK_NAME       "\\DosDevices\\CgBoxDevice"
#define CG_BOX_DEVICE_LINK_NAME_USER  "\\\\.\\CgBoxDevice"
#endif

namespace io {
namespace ctl {

typedef enum {
    STATUS_INJECTED_SUCCESS = 0x1000,
    STATUS_INJECTED_FAILED  = 0x1001
} CG_BOX_STATUS;

typedef struct CgBoxIoCtlCgConfigResp {
    ULONG IoControlCode;
    INT   status;
} CG_BOX_IOCTL_CG_CONFIG_RESP;

typedef struct CgBoxIoCtlGameCgSetPidResp {
    ULONG IoControlCode;
    INT   status;
} CG_BOX_IOCTL_GAME_CG_SET_PID_RESP;

typedef struct CgBoxIoCtlGameCgQueryPidResp {
    ULONG PID;
    ULONG IoControlCode;
    INT   status;
} CG_BOX_IOCTL_GAME_CG_QUERY_PID_RESP;

typedef struct CgBoxIoCtlCgConfigReq { // Use IoCtl as data transfer module
    unsigned int magic;
    unsigned int version;
    unsigned int offloadsize;

    int          load_library_ex_w_offset;
    int          write_file_offset;
    int          create_file_offset;
    int          close_handle_offset;
    int          get_current_process_id_offset;
    int          device_io_control_offset;

    WCHAR        HookDllName[CG_MAX_FILE_NAME];
    CHAR         GameName[CG_MAX_FILE_NAME];
    CHAR         CGBoxDllPath[CG_MAX_FILE_NAME];
} CG_BOX_IOCTL_CG_CONFIG_REQ;

typedef struct CgBoxIoCtlGameCgSetPidReq {
    unsigned int magic;
    unsigned int version;
    unsigned int offloadsize;

    ULONG        PID;
} CG_BOX_IOCTL_GAME_CG_SET_PID_REQ;

}  // namespace ctl
}  // namespace io

#endif
