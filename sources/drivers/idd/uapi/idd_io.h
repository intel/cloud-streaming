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
#include <minwindef.h>
#include <winioctl.h>
#include <guiddef.h>

typedef enum _IDD_STATUS
{
    IDD_STATUS_SUCCESS,
    IDD_STATUS_ACCESS_DENIED,
    IDD_INVALID_PARAM,
    IDD_INVALID_HANDLE
}IDD_STATUS;

#define IOCTL_IDD_UPDATE_LUID                CTL_CODE(IOCTL_CHANGER_BASE, \
                                                       0x8001, \
                                                       METHOD_BUFFERED, \
                                                       FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _update_luid {
    LUID luid;
} IDD_UPDATE_LUID, * PIDD_UPDATE_LUID;


