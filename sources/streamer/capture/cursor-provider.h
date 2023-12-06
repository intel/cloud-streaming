// Copyright (C) 2023 Intel Corporation
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

#include "frame.h"

#include <memory>
#include <vector>

struct CursorState {
    // cursor presence on the screen
    bool visible;
    // cursor shape is always R8G8B8A8
    bool shape_present;
    uint32_t shape_width;
    uint32_t shape_height;
    uint32_t shape_pitch;
    // shape data - size is (height * pitch)
    // shape data - normal color (use alpha blend for rendering)
    std::vector<unsigned char> shape_data;
    // shape data - inverted color (use invert blend for rendering)
    std::vector<unsigned char> shape_xor_data;
};

/**
 * @brief      This class describes generic cursor provider interface.
 *             User can obtain cursor state calling receive_cursor() in a loop.
 */
struct CursorProvider {
    
    virtual ~CursorProvider() = default;

    /**
     * @brief      Block thread and wait for cursor update.
     *             If timeout is 0 - return state immediately.
     *             Called by user to acquire cursor state.
     *
     * @param      cursor_desc  cursor description
     * @param[in]  timeout_ms   timeout in milliseconds
     *
     * @return     0 and cursor state, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new frame arrives
     *             E_FAIL, on error
     */
    virtual HRESULT receive_cursor(CursorState& cursor_state, UINT timeout_ms) = 0;
};
