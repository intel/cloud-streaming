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

#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <cassert>

#include "Utility.h"

struct Adapter
{
    tstring devID;
    tstring devName;
    tstring devHardwareID;
    GUID    devGuid = {};
    int     devIndex = -1;
};

struct adapter_target_info_t
{
    tstring                      pattern;
    bool                         is_an_inverted_target;
    tstring                      target_device_id;
    ENABLE_DISABLE_PATTERN_TYPES pattern_type;
};

std::vector<Adapter> GetAdapterList(bool verbose);
size_t GetNumIddCompatibleAdapters(std::vector<Adapter> &adapter_list);
void DisableDisplayAdapter(tstring Pattern, bool verbose = true);
void EnableDisplayAdapter(tstring Pattern, bool verbose = true);
