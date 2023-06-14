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

// This file is dedicated to list GUIDs which requires initialization. Keep
// _only_ GUIDs here so you can add this header as a last item in your
// headers list and initialize it with initguid.h as needed.

#include <guiddef.h>

// {4d36e968-e325-11ce-bfc1-08002be10318}
DEFINE_GUID(DISPLAY_GUID,
    0x4d36e968, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);
