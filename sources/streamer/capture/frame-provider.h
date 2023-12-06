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

/**
 * @brief      This class describes generic frame provider interface.
 *             User can obtain last captured frame by calling receive_frame() in a loop.
 */
struct FrameProvider {

    virtual ~FrameProvider() = default;

    /**
     * @brief      Start frame capture
     *
     * @return     0, on success
     *             HRESULT, om error
     */
    virtual HRESULT start() = 0;

    /**
     * @brief      Stop frame capture
     */
    virtual void stop() = 0;

    /**
     * @brief      Block thread and wait for a new frame with timeout.
     *             Called by user to acquire new frame.
     *
     * @param      frame       frame object
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0 and frame object, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new frame arrives
     *             E_FAIL, on error
     */
    virtual HRESULT receive_frame(std::shared_ptr<Frame>& frame, UINT timeout_ms) = 0;
};
