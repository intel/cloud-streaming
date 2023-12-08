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

#include "dx-utils.h"

#include <dxgi1_2.h>
#include <atlcomcli.h>

HRESULT utils::enum_adapter_by_luid(IDXGIAdapter** pp_adapter, const LUID& luid) {
    if (pp_adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": adapter is nullptr\n");
        return E_INVALIDARG;
    }
    if (*pp_adapter != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // create factory
    CComPtr<IDXGIFactory4> factory;
    HRESULT result = CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory));
    if (FAILED(result) || factory == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateDXGIFactory1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<IDXGIAdapter> adapter;
    result = factory->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter), reinterpret_cast<void**>(&adapter));
    if (FAILED(result) || adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIFactory4->EnumAdapterByLuid failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    *pp_adapter = adapter.Detach();
    return S_OK;
}

HRESULT utils::enum_adapter_by_display_name(IDXGIAdapter** pp_adapter, IDXGIOutput** pp_output, const std::wstring& display_device_name) {
    if (pp_adapter == nullptr && pp_output == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": both adapter and output are nullptr\n");
        return E_INVALIDARG;
    }
    if (pp_adapter != nullptr && *pp_adapter != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (pp_output != nullptr && *pp_output != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // create factory
    CComPtr<IDXGIFactory1> factory;
    HRESULT result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(result) || factory == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateDXGIFactory1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // find output device and parent adapter matching desc
    CComPtr<IDXGIAdapter1> adapter;
    CComPtr<IDXGIOutput> output;
    for (UINT adapter_idx = 0; SUCCEEDED(factory->EnumAdapters1(adapter_idx, &adapter)); ++adapter_idx) {
        if (adapter == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIFactory1->EnumAdapters1 failed, adapter_idx = %d, result = 0x%08x\n", adapter_idx, result);
            return E_FAIL;
        }

        for (UINT output_idx = 0; SUCCEEDED(adapter->EnumOutputs(output_idx, &output)); ++output_idx) {
            if (output == nullptr) {
                ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->EnumOutputs failed, adapter_idx = %d, output_idx = %d, result = 0x%08x\n", adapter_idx, output_idx, result);
                return E_FAIL;
            }

            DXGI_OUTPUT_DESC desc = {};
            result = output->GetDesc(&desc);
            if (FAILED(result)) {
                ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutput->GetDesc failed, adapter_idx = %d, output_idx = %d, result = 0x%08x\n", adapter_idx, output_idx, result);
                return E_FAIL;
            }

            std::wstring device_name(desc.DeviceName);
            if (display_device_name == device_name)
                break; // found matching display output

            // reset output
            output = nullptr;
        }

        if (output != nullptr)
            break; // found matching output
        else
            adapter = nullptr; // reset adapter
    }

    if (adapter == nullptr || output == nullptr)
        return DXGI_ERROR_NOT_FOUND;

    if (pp_adapter != nullptr)
        *pp_adapter = adapter.Detach();
    if (pp_output != nullptr)
        *pp_output = output.Detach();

    return S_OK;
}

HRESULT utils::enum_adapter_by_vendor(IDXGIAdapter** pp_adapter, UINT vendor_id) {
    if (pp_adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": adapter is nullptr\n");
        return E_INVALIDARG;
    }
    if (*pp_adapter != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // create factory
    CComPtr<IDXGIFactory1> factory;
    HRESULT result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(result) || factory == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateDXGIFactory1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // find output device and parent adapter matching desc
    CComPtr<IDXGIAdapter1> adapter;
    for (UINT adapter_idx = 0; SUCCEEDED(factory->EnumAdapters1(adapter_idx, &adapter)); ++adapter_idx) {
        if (adapter == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIFactory1->EnumAdapters1 failed, adapter_idx = %d, result = 0x%08x\n", adapter_idx, result);
            return E_FAIL;
        }

        DXGI_ADAPTER_DESC adapter_desc = {};
        result = adapter->GetDesc(&adapter_desc);
        if (adapter_desc.VendorId == vendor_id)
            break;

        adapter = nullptr; // reset adapter
    }

    if (adapter == nullptr)
        return DXGI_ERROR_NOT_FOUND;

    if (pp_adapter != nullptr)
        *pp_adapter = adapter.Detach();

    return S_OK;
}

HRESULT utils::enum_primary_display(IDXGIAdapter** pp_adapter, IDXGIOutput** pp_output) {
    if (pp_adapter == nullptr && pp_output == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": both adapter and output are nullptr\n");
        return E_INVALIDARG;
    }
    if (pp_adapter != nullptr && *pp_adapter != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (pp_output != nullptr && *pp_output != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // create factory
    CComPtr<IDXGIFactory1> factory;
    HRESULT result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(result) || factory == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateDXGIFactory1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<IDXGIAdapter1> adapter;
    result = factory->EnumAdapters1(0, &adapter);
    if (FAILED(result) || adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIFactory1->EnumAdapters1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<IDXGIOutput> output;
    result = adapter->EnumOutputs(0, &output);
    if (FAILED(result) || output == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIFactory1->EnumAdapters1 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    if (adapter == nullptr || output == nullptr)
        return DXGI_ERROR_NOT_FOUND;

    if (pp_adapter != nullptr)
        *pp_adapter = adapter.Detach();
    if (pp_output != nullptr)
        *pp_output = output.Detach();

    return S_OK;
}

HRESULT utils::create_d3d11_device(IDXGIAdapter* adapter, ID3D11Device5** pp_device, ID3D11DeviceContext4** pp_context, ID3D11Multithread** pp_context_lock) {
    if (pp_device != nullptr && *pp_device != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (pp_context != nullptr && *pp_context != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (pp_context_lock != nullptr && *pp_context_lock != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    CComPtr<ID3D11Device> device;
    CComPtr<ID3D11DeviceContext> context;

    UINT device_flags = 0; // D3D11_CREATE_DEVICE_DEBUG;

    // shared NT handles require feature level 11.1
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_1;
    HRESULT result = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr /* sw_rasterizer_handle */,
        device_flags, &feature_level, 1, D3D11_SDK_VERSION, &device,
        nullptr /* feature_level_out */, &context);
    if (FAILED(result) || device == nullptr || context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": D3D11CreateDevice failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    feature_level = device->GetFeatureLevel();
    if (feature_level < D3D_FEATURE_LEVEL_11_1) {
        ga_logger(Severity::ERR, __FUNCTION__ ": D3D11 device does not support feature level 11.1\n");
        return E_FAIL;
    }

    CComPtr<ID3D11Device5> device5;
    result = device->QueryInterface<ID3D11Device5>(&device5);
    if (FAILED(result) || device5 == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D11DeviceContext4> context4;
    result = context->QueryInterface<ID3D11DeviceContext4>(&context4);
    if (FAILED(result) || context4 == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // set multithread protection
    CComPtr<ID3D11Multithread> context_lock;
    result = context->QueryInterface<ID3D11Multithread>(&context_lock);
    if (FAILED(result) || context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    std::ignore = context_lock->SetMultithreadProtected(true);

    if (pp_device != nullptr)
        *pp_device = device5.Detach();
    if (pp_context != nullptr)
        *pp_context = context4.Detach();
    if (pp_context_lock != nullptr)
        *pp_context_lock = context_lock.Detach();

    return S_OK;
}

HRESULT utils::create_d3d12_device(IDXGIAdapter* adapter, ID3D12Device** pp_device) {
    if (pp_device != nullptr && *pp_device != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    CComPtr<ID3D12Device> device;
    HRESULT result = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0,
        __uuidof(ID3D12Device), reinterpret_cast<void**>(&device));
    if (FAILED(result) || device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": D3D12CreateDevice failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    if (pp_device != nullptr)
        *pp_device = device.Detach();

    return S_OK;
}

LUID utils::get_adapter_luid_from_device(ID3D11Device* device) {
    LUID luid = {};

    if (device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return luid;
    }

    CComPtr<IDXGIDevice> dxgi_device;
    HRESULT result = device->QueryInterface<IDXGIDevice>(&dxgi_device);
    if (FAILED(result) || dxgi_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->QueryInterface failed, result = 0x%08x\n", result);
        return luid;
    }

    CComPtr<IDXGIAdapter> adapter;
    result = dxgi_device->GetAdapter(&adapter);
    if (FAILED(result) || adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIDevice->GetAdapter failed, result = 0x%08x\n", result);
        return luid;
    }

    DXGI_ADAPTER_DESC adapter_desc = {};
    result = adapter->GetDesc(&adapter_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->GetDesc failed, result = 0x%08x\n", result);
        return luid;
    }

    return adapter_desc.AdapterLuid;
}

LUID utils::get_adapter_luid_from_device(ID3D12Device* device) {
    LUID luid = {};

    if (device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return luid;
    }

    return device->GetAdapterLuid();
}

bool utils::is_same_luid(const LUID& lhs, const LUID& rhs) {
    return lhs.LowPart == rhs.LowPart && lhs.HighPart == rhs.HighPart;
}
