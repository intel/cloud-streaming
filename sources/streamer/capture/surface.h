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

#include <d3d11_4.h>
#include <d3d12.h>

/**
 * @brief      This class describes a surface interface
 */
struct Surface {

    virtual ~Surface() = default;

    /**
     * @brief      Open D3D11 texture allocation on specified device.
     *             If device matches the one surface was created on,
     *             return underlying texture reference.
     *
     * @param[in]  device      destination device
     * @param[out] pp_texture  output texture
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    virtual HRESULT open_shared_texture(ID3D11Device* device, ID3D11Texture2D** pp_texture) = 0;

    /**
     * @brief      Open D3D12 resource allocation on specified device.
     *             If device matches the one surface was created on,
     *             return underlying resource reference.
     *
     * @param[in]  device      destination device
     * @param[out] pp_resource output resource
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    virtual HRESULT open_shared_resource(ID3D12Device* device, ID3D12Resource** pp_resource) = 0;

    /**
     * @brief      Signal gpu event
     *
     * @param      fence         fence object
     * @param[in]  shared_fence  shared fence handle
     * @param[in]  value         fence value
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    virtual HRESULT signal_gpu_event(ID3D11Fence* fence, HANDLE shared_fence, UINT64 value) = 0;

    /**
     * @brief      Signal gpu event
     *
     * @param      fence         fence object
     * @param[in]  shared_fence  shared fence handle
     * @param[in]  value         fence value
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    virtual HRESULT signal_gpu_event(ID3D12Fence* fence, HANDLE shared_fence, UINT64 value) = 0;

    /**
     * @brief      Block current thread and wait for gpu fence reach last stored value
     *
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before gpu fence clears
     *             E_FAIL, on error
     */
    virtual HRESULT wait_gpu_event_cpu(UINT timeout_ms) = 0;

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
    virtual HRESULT wait_gpu_event_gpu(ID3D11DeviceContext* context) = 0;

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
    virtual HRESULT wait_gpu_event_gpu(ID3D12CommandQueue* queue) = 0;


    /**
     * @return     return adapter LUID on which surface was allocated
     */
    virtual LUID get_device_luid() = 0;

    /**
     * @return     return surface width
     */
    virtual UINT get_width() = 0;

    /**
     * @return     return surface height
     */
    virtual UINT get_height() = 0;

    /**
     * @return     return surface format
     */
    virtual DXGI_FORMAT get_format() = 0;
};
