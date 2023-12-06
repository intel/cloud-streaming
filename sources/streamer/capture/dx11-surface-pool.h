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

#include "surface-pool.h"

#include <atlcomcli.h>
#include <d3d11.h>

#include <list>
#include <mutex>

/**
 * @brief      This class describes D3D11 surface pool
 */
class DX11SurfacePool : public SurfacePool {
public:
    struct Desc {
        // D3D11 device used for surface allocation
        ID3D11Device* device = nullptr;

        // D3D11 texture description
        D3D11_TEXTURE2D_DESC texture_desc = {};
    };

    virtual ~DX11SurfacePool() = default;

    /**
     * @brief      Create new surface pool instance
     *
     * @param      desc  pool description
     *
     * @return     new instance, on success
     *             nullptr, on error
     */
    static std::unique_ptr<DX11SurfacePool> create(DX11SurfacePool::Desc& desc);

    /**
     * @brief      Create new surface or return free surface
     *
     * @return     surface object, on success
     *             nullptr, on error
     */
    std::unique_ptr<Surface> acquire() override;

    /**
     * @brief      Return surface to pool. If surface type or description
     *             does not match pool desc surface is destroyed.
     *
     * @param[in]  surface  surface object
     */
    void release(std::unique_ptr<Surface> surface) override;

    /**
     * @return     Returns D3D11 texture description.
     */
    const D3D11_TEXTURE2D_DESC& get_texture_desc() const { return m_texture_desc; }

private:
    DX11SurfacePool() = default;

private:
    std::recursive_mutex m_acquire_lock;
    std::list<std::unique_ptr<Surface>> m_free_list;

    CComPtr<ID3D11Device> m_device;
    D3D11_TEXTURE2D_DESC m_texture_desc = {};
};
