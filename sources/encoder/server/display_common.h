// Copyright (C) 2018-2022 Intel Corporation
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

#ifndef _DISPLAY_COMMON_H_
#define _DISPLAY_COMMON_H_

#include <stdint.h>
#include "api/irrv.h"

#define MAX_HANDLE_COUNT 4
typedef struct _disp_res {
    vhal::client::buffer_handle_t local_handle;

    int             width;
    int             height;
    int             drm_format;
    int             android_format;
    int             prime_fds[MAX_HANDLE_COUNT];
    int             strides[MAX_HANDLE_COUNT];
    int             offsets[MAX_HANDLE_COUNT];
    unsigned int    seq_no;
    uint64_t        format_modifiers[MAX_HANDLE_COUNT];

    uint32_t        gem_handles[MAX_HANDLE_COUNT];
    uint32_t        fb_ids[MAX_HANDLE_COUNT];

    irr_surface_t*  surface;

    bool is_reged;
} disp_res_t;

#endif
