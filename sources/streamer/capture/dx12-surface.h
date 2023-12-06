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
#include "event-queue.h"

#include <atlcomcli.h>
#include <d3d11_4.h>
#include <d3d12.h>

#include <deque>
#include <memory>
#include <mutex>

/**
 * @brief      This class describes D3D12 surface implementation
 */
class DX12Surface : public Surface {
public:
    virtual ~DX12Surface();

    /**
     * @brief      Allocate new D3D12 resource and return surface object
     *             This function uses ID3D12Device::CreateCommittedResource for allocation
     *
     * @param      device         D3D12 device to allocate
     * @param[in]  heap_props     D3D12 heap properties
     * @param[in]  heap_flags     D3D12 heap flags
     * @param[in]  resource_desc  D3D12 resource description
     *
     * @return     surface object, on success
     *             nullptr, on error
     */
    static std::unique_ptr<DX12Surface> create(ID3D12Device* device,
        const D3D12_HEAP_PROPERTIES* heap_props, D3D12_HEAP_FLAGS heap_flags,
        const D3D12_RESOURCE_DESC* resource_desc);

    /**
     * @return     Returns D3D12 device used for resource allocation
     */
    ID3D12Device* get_device() const { return m_device; }

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

    /**
     * @brief      Open D3D11 texture allocation on specified device.
     *             If device matches the one surface was created on,
     *             return underlying texture reference.
     *             Cross-adapted sharing is not supported.
     *
     * @param[in]  device      destination device
     * @param[out] pp_texture  output texture
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT open_shared_texture(ID3D11Device* device, ID3D11Texture2D** pp_texture) override;

    /**
     * @brief      Open D3D12 resource allocation on specified device.
     *             If device matches the one surface was created on,
     *             return underlying resource reference.
     *             Cross-adapted sharing is not supported.
     *
     * @param[in]  device      destination device
     * @param[out] pp_texture  output texture
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT open_shared_resource(ID3D12Device* device, ID3D12Resource** pp_resource) override;

    /**
     * @brief      Signal gpu event, both fence and shared handle must point to the same object
     *             Shared handle must be created by ID3D11Fence::CreateSharedHandle
     *
     * @param      fence         fence object
     * @param[in]  shared_fence  shared fence handle
     * @param[in]  value         fence value
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT signal_gpu_event(ID3D11Fence* fence, HANDLE shared_fence, UINT64 value) override;

    /**
     * @brief      Signal gpu event, both fence and shared handle must point to the same object
     *             Shared handle must be created by ID3D12Device::CreateSharedHandle
     *
     * @param      fence         fence object
     * @param[in]  shared_fence  shared fence handle
     * @param[in]  value         fence value
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT signal_gpu_event(ID3D12Fence* fence, HANDLE shared_fence, UINT64 value) override;

    /**
     * @brief      Block current thread and wait for gpu fence reach last stored value
     *
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before gpu fence clears
     *             E_FAIL, on error
     */
    HRESULT wait_gpu_event_cpu(UINT timeout_ms) override;

    /**
     * @brief      Queue GPU wait until fence reaches last stored value.
     *             Command queue will wait (during which time no work is executed)
     *             until the fence reaches the requested value.
     *             Because a wait is being queued, this API returns immediately.
     *
     * @param      context  d3d11 device context
     *
     * @return     0, on success
     *             E_INVALIDARG, if context is nullptr
     *             E_FAIL, on error
     */
    HRESULT wait_gpu_event_gpu(ID3D11DeviceContext* context) override;

    /**
     * @brief      Queue GPU wait until fence reaches last stored value.
     *             Command queue will wait (during which time no work is executed)
     *             until the fence reaches the requested value.
     *             Because a wait is being queued, this API returns immediately.
     *
     * @param      context  d3d12 command queue
     *
     * @return     0, on success
     *             E_INVALIDARG, if context is nullptr
     *             E_FAIL, on error
     */
    HRESULT wait_gpu_event_gpu(ID3D12CommandQueue* queue) override;

    /**
     * @return     return adapter LUID on which surface was allocated
     */
    LUID get_device_luid() override;

    /**
     * @return     return surface width
     */
    UINT get_width() override;

    /**
     * @return     return surface height
     */
    UINT get_height() override;

    /**
     * @return     return surface format
     */
    DXGI_FORMAT get_format() override;

private:
    DX12Surface() = default;

private:
    // resource and device ref
    CComPtr<ID3D12Device> m_device;
    CComPtr<ID3D12Resource> m_resource;
    D3D12_HEAP_PROPERTIES m_heap_props = {};
    D3D12_HEAP_FLAGS m_heap_flags = {};
    D3D12_RESOURCE_DESC m_resource_desc = {};

    // shared resource handle
    HANDLE m_shared_handle = nullptr;
    
    // gpu event queue
    std::recursive_mutex m_event_queue_lock;
    EventQueue m_event_queue;
};
