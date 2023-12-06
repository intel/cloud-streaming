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
#include <d3d12.h>

#include <list>
#include <mutex>

/**
 * @brief      This class describes D3D12 resource pool
 */
class DX12SurfacePool : public SurfacePool {
public:
    struct Desc {
        // D3D12 device used for surface allocation
        ID3D12Device* device = nullptr;

        // D3D12 heap properties
        D3D12_HEAP_PROPERTIES heap_props = {};

        // D3D12 heap flags
        D3D12_HEAP_FLAGS heap_flags = {};

        // D3D12 resource desc
        D3D12_RESOURCE_DESC resource_desc = {};
    };

    virtual ~DX12SurfacePool() = default;

    /**
     * @brief      Create new surface pool instance
     *
     * @param      desc  pool description
     *
     * @return     new instance, on success
     *             nullptr, on error
     */
    static std::unique_ptr<DX12SurfacePool> create(DX12SurfacePool::Desc& desc);

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
     * @return     Returns D3D12 heap properties
     */
    const D3D12_HEAP_PROPERTIES& get_heap_props() const { return m_heap_props; }

    /**
     * @return     Returns D3D12 heap flags
     */
    const D3D12_HEAP_FLAGS& get_heap_flags() const { return m_heap_flags; }

    /**
     * @return     Returns D3D12 resource description
     */
    const D3D12_RESOURCE_DESC& get_resource_desc() const { return m_resource_desc; }

private:
    DX12SurfacePool() = default;

private:
    std::recursive_mutex m_acquire_lock;
    std::list<std::unique_ptr<Surface>> m_free_list;

    CComPtr<ID3D12Device> m_device;
    D3D12_HEAP_PROPERTIES m_heap_props = {};
    D3D12_HEAP_FLAGS m_heap_flags = {};
    D3D12_RESOURCE_DESC m_resource_desc = {};
};
