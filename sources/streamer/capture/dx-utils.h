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

#include <atlcomcli.h>
#include <dxgi.h>
#include <d3d11_4.h>
#include <d3d12.h>

#include <string>

namespace utils {
    /**
     * @brief      Enumerate DXGI adapter matching target LUID
     *
     * @param[out] pp_adapter  pointer to DXGI adapter interface
     * @param[in]  luid        target adapter luid
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT enum_adapter_by_luid(IDXGIAdapter** pp_adapter, const LUID& luid);

    /**
     * @brief      Enumerate DXGI output and adapter matching display name
     *
     * @param[out] pp_adapter           pointer to DXGI adapter interface
     * @param[out] pp_output            pointer to DXGI output interface
     * @param[in]  display_device_name  target display device name
     *
     * @return     0, on success
     *             DXGI_ERROR_NOT_FOUND, if output with given display name is not found
     *             E_FAIL, on error
     */
    HRESULT enum_adapter_by_display_name(IDXGIAdapter** pp_adapter, IDXGIOutput** pp_output, 
        const std::wstring& display_device_name);

    /**
     * @brief      Enumerate DXGI adapter matching vendor id
     *
     * @param[out] pp_adapter  pointer to DXGI adapter interface
     * @param[in]  vendor_id   vendor identifier
     *
     * @return     0, on success
     *             DXGI_ERROR_NOT_FOUND, if output with given display name is not found
     *             E_FAIL, on error
     */
    HRESULT enum_adapter_by_vendor(IDXGIAdapter** pp_adapter, UINT vendor_id);

    /**
     * @brief      Enumerate DXGI primary output and adapter
     *
     * @param[out] pp_adapter  pointer to DXGI adapter interface
     * @param[out] pp_output   pointer to DXGI output interface
     *
     * @return     0, on success
     *             DXGI_ERROR_NOT_FOUND, if output with given display name is not found
     *             E_FAIL, on error
     */
    HRESULT enum_primary_display(IDXGIAdapter** pp_adapter, IDXGIOutput** pp_output);

    /**
     * @brief      Create D3D11 device and context
     *
     * @param[in]  adapter          target adapter
     * @param[out] pp_device        pointer to D3D11 device interface
     * @param[out] pp_context       pointer to D3D11 device context interface
     * @param[out] pp_context_lock  pointer to D3D11 device context lock interface
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT create_d3d11_device(IDXGIAdapter* adapter, ID3D11Device5** pp_device,
        ID3D11DeviceContext4** pp_context, ID3D11Multithread** pp_context_lock);

    /**
     * @brief      Create D3D12 device and context
     *
     * @param[in]  adapter    target adapter
     * @param[out] pp_device  pointer to D3D12 device interface
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT create_d3d12_device(IDXGIAdapter* adapter, ID3D12Device** pp_device);

    /**
     * @brief      Get LUID of the adapter associated with device
     *
     * @param[in]  device  D3D11 device interface
     *
     * @return     adapter luid
     */
    LUID get_adapter_luid_from_device(ID3D11Device* device);

    /**
     * @brief      Get LUID of the adapter associated with device
     *
     * @param[in]  device  D3D11 device interface
     *
     * @return     adapter luid
     */
    LUID get_adapter_luid_from_device(ID3D12Device* device);

    /**
     * @return     true, if LUIDs are equal
     *             false, otherwise
     */
    bool is_same_luid(const LUID& lhs, const LUID& rhs);

}; // namespace utils
