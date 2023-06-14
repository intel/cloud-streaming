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

#include "Utility.h"

using device_settings_pair = std::pair<DISPLAY_DEVICE, DEVMODE>;
using list_of_settings = std::list<device_settings_pair>;

struct display_target_info_t
{
    tstring                      pattern;
    bool                         is_an_inverted_target;
    tstring                      target_device_name;
    ENABLE_DISABLE_PATTERN_TYPES pattern_type;
    int32_t                      default_width;
    int32_t                      default_height;
};

// This function actually queries topology of screen and returns it back. No changes to displays arrangement happen here
list_of_settings QueryActiveSettings(bool verbose = true, bool extract_only_attached = true);

int RearrangeDisplays(list_of_settings active_settings);
int DisableDisplay(tstring Pattern, list_of_settings active_settings);
int EnableDisplay(tstring Pattern, list_of_settings active_settings);
