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

#include "ga-common.h"

#include "surface.h"

#include <memory>

/**
 * @brief      This class describes generic surface pool interface.
 *             Surface pool does not hold references to surfaces in use.
 *             User must track manualy which pool surface was allocated on.
 */
struct SurfacePool {

    virtual ~SurfacePool() = default;

    /**
     * @brief      Create new surface or return free surface
     *
     * @return     surface object, on success
     *             nullptr, on error
     */
    virtual std::unique_ptr<Surface> acquire() = 0;

    /**
     * @brief      Return surface to pool. If surface type or description
     *             does not match pool desc surface is destroyed.
     *
     * @param[in]  surface  surface object
     */
    virtual void release(std::unique_ptr<Surface> surface) = 0;
};
