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
#include "surface-pool.h"

#include <memory>

/**
 * @brief      This class describes a frame object which encapsulates a surface.
 *             Surface is automatically released upon destruction of this object.
 *             If pool reference is provided, surface returns to pool for re-use.
 */
class Frame {
public:
    virtual ~Frame();

    /**
     * @brief      Create frame object from surface
     *
     * @param[in]  surface  surface object
     * @param[in]  pool     reference to surface pool surface was allocated on
     *
     * @return     new frame object on success,
     *             nullptr, on error
     */
    static std::unique_ptr<Frame> create(std::unique_ptr<Surface> surface, std::weak_ptr<SurfacePool> pool);

    /**
     * @return     Return underlying surface
     */
    Surface* get_surface() { return m_surface.get(); }

private:
    Frame() = default;

    std::unique_ptr<Surface> m_surface;
    std::weak_ptr<SurfacePool> m_pool;
};
