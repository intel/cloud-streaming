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

#include "dx11-surface.h"

#include "dx-utils.h"

#include <dxgi1_2.h>
#include <d3d11_4.h>

DX11Surface::~DX11Surface() {
    // wait queued events
    wait_gpu_event_cpu(INFINITE);

    const bool is_shared_nt_handle = m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    if (is_shared_nt_handle)
        if (m_shared_handle != nullptr)
            CloseHandle(m_shared_handle);
}

std::unique_ptr<DX11Surface> DX11Surface::create(ID3D11Device* device, D3D11_TEXTURE2D_DESC* desc) {
    if (device == nullptr || desc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid arguments\n");
        return nullptr;
    }

    // create texture
    CComPtr<ID3D11Texture2D> texture;
    HRESULT result = device->CreateTexture2D(desc, nullptr, &texture);
    if (FAILED(result) || texture == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->CreateTexture2D failed, result = 0x%08x\n", result);
        return nullptr;
    }

    const bool is_shared_misc = desc->MiscFlags & D3D11_RESOURCE_MISC_SHARED;
    const bool is_shared_nt_handle = desc->MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    auto instance = std::unique_ptr<DX11Surface>(new DX11Surface);
    instance->m_device = device;
    instance->m_texture = texture;
    instance->m_desc = *desc;

    // create shared handle
    if (is_shared_nt_handle) {
        CComPtr<IDXGIResource1> resource;
        result = instance->m_texture->QueryInterface<IDXGIResource1>(&resource);
        if (FAILED(result) || resource == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Texture2D->QueryInterface failed, result = 0x%08x\n", result);
            return nullptr;
        }

        DWORD access_flags = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE;
        result = resource->CreateSharedHandle(nullptr, access_flags, nullptr, &instance->m_shared_handle);
        if (FAILED(result) || instance->m_shared_handle == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIResource1->CreateSharedHandle failed, result = 0x%08x\n", result);
            return nullptr;
        }
    } else if (is_shared_misc) {
        CComPtr<IDXGIResource> resource;
        result = instance->m_texture->QueryInterface<IDXGIResource>(&resource);
        if (FAILED(result) || resource == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Texture2D->QueryInterface failed, result = 0x%08x\n", result);
            return nullptr;
        }

        result = resource->GetSharedHandle(&instance->m_shared_handle);
        if (FAILED(result) || instance->m_shared_handle == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIResource->GetSharedHandle failed, result = 0x%08x\n", result);
            return nullptr;
        }
    }

    return instance;
}

HRESULT DX11Surface::open_shared_texture(ID3D11Device* device, ID3D11Texture2D** pp_texture) {
    if (device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_INVALIDARG;
    }
    if (pp_texture == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": texture is nullptr\n");
        return E_INVALIDARG;
    }
    if (*pp_texture != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // if device is the same return texture ref
    if (device == m_device) {
        CComPtr<ID3D11Texture2D> clone = m_texture;
        *pp_texture = clone.Detach();
        return S_OK;
    }

    // texture is not shared
    if (m_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": shared handle is nullptr\n");
        return E_FAIL;
    }

    auto src_luid = utils::get_adapter_luid_from_device(m_device);
    auto dst_luid = utils::get_adapter_luid_from_device(device);
    if (!utils::is_same_luid(src_luid, dst_luid)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": cross adapter sharing is not allowed\n");
        return E_FAIL;
    }

    const bool is_shared_nt_handle = m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    const bool is_shared_misc = m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED;

    // open shared texture
    if (is_shared_nt_handle) {
        CComPtr<ID3D11Device1> device1;
        HRESULT result = device->QueryInterface<ID3D11Device1>(&device1);
        if (FAILED(result) || device1 == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->QueryInterface failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        CComPtr<ID3D11Texture2D> shared_texture;
        result = device1->OpenSharedResource1(m_shared_handle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&shared_texture));
        if (FAILED(result) || shared_texture == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device1->OpenSharedResource1 failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        *pp_texture = shared_texture.Detach();
        return S_OK;
    }

    if (is_shared_misc) {
        CComPtr<ID3D11Texture2D> shared_texture;
        HRESULT result = device->OpenSharedResource(m_shared_handle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&shared_texture));
        if (FAILED(result) || shared_texture == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->OpenSharedResource failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        *pp_texture = shared_texture.Detach();
        return S_OK;
    }

    return E_FAIL;
}

HRESULT DX11Surface::open_shared_resource(ID3D12Device* device, ID3D12Resource** pp_resource) {
    if (device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_INVALIDARG;
    }
    if (pp_resource == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": resource is nullptr\n");
        return E_INVALIDARG;
    }
    if (*pp_resource != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // resource is not shared
    if (m_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": shared handle is nullptr\n");
        return E_FAIL;
    }

    auto src_luid = utils::get_adapter_luid_from_device(m_device);
    auto dst_luid = utils::get_adapter_luid_from_device(device);
    if (!utils::is_same_luid(src_luid, dst_luid)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": cross adapter sharing is not allowed\n");
        return E_FAIL;
    }

    const bool is_shared_nt_handle = m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    // open shared resource
    if (is_shared_nt_handle) {
        // open shared resource
        CComPtr<ID3D12Resource> shared_resource;
        HRESULT result = device->OpenSharedHandle(m_shared_handle, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&shared_resource));
        if (FAILED(result) || shared_resource == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->OpenSharedHandle failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        *pp_resource = shared_resource.Detach();
        return S_OK;
    }

    return E_FAIL;
}

HRESULT DX11Surface::signal_gpu_event(ID3D11Fence* fence, HANDLE shared_fence, UINT64 value) {
    if (fence == nullptr || shared_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": fence is nullptr\n");
        return E_FAIL;
    }

    // duplicate fence handle
    HANDLE fence_handle = nullptr;
    bool ok = DuplicateHandle(GetCurrentProcess(), shared_fence,
        GetCurrentProcess(), &fence_handle, 0, false, DUPLICATE_SAME_ACCESS);
    if (!ok) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": DuplicateHandle failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    std::lock_guard lk(m_event_queue_lock);

    // acquire new event
    m_event_queue.push_back(fence_handle, value);
    auto& gpu_event = m_event_queue.back();

    if (gpu_event.event_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }

    // signal cpu side event
    HRESULT result = fence->SetEventOnCompletion(value, gpu_event.event_handle);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Fence->SetEventOnCompletion failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    gpu_event.d3d11_fence = fence;
    gpu_event.signalled = true;

    return S_OK;
}

HRESULT DX11Surface::signal_gpu_event(ID3D12Fence* fence, HANDLE shared_fence, UINT64 value) {
    if (fence == nullptr || shared_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": fence is nullptr\n");
        return E_FAIL;
    }

    // duplicate fence handle
    HANDLE fence_handle = nullptr;
    bool ok = DuplicateHandle(GetCurrentProcess(), shared_fence,
        GetCurrentProcess(), &fence_handle, 0, false, DUPLICATE_SAME_ACCESS);
    if (!ok) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": DuplicateHandle failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    std::lock_guard lk(m_event_queue_lock);

    // acquire new event
    m_event_queue.push_back(fence_handle, value);
    auto& gpu_event = m_event_queue.back();

    if (gpu_event.event_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }

    // signal cpu side event
    HRESULT result = fence->SetEventOnCompletion(value, gpu_event.event_handle);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Fence->SetEventOnCompletion failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    gpu_event.d3d12_fence = fence;
    gpu_event.signalled = true;

    return S_OK;
}

HRESULT DX11Surface::wait_gpu_event_cpu(UINT timeout_ms) {
    using namespace std::chrono;
    
    std::unique_lock lk(m_event_queue_lock);
    // if no events signalled - return immediately
    if (m_event_queue.empty())
        return S_OK;

    // wait all queued events
    while (!m_event_queue.empty()) {
        auto& gpu_event = m_event_queue.front();
        if (gpu_event.event_handle == nullptr) {
            m_event_queue.pop_front();
            continue;
        }

        auto wait_start = steady_clock::now();
        auto wait_result = WaitForSingleObject(gpu_event.event_handle, timeout_ms);
        auto wait_end = steady_clock::now();

        if (wait_result == WAIT_TIMEOUT)
            return DXGI_ERROR_WAIT_TIMEOUT;

        gpu_event.signalled = false;

        if (wait_result != WAIT_OBJECT_0) {
            HRESULT result = HRESULT_FROM_WIN32(GetLastError());
            ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", result);
        }

        auto wait_time = wait_end - wait_start;
        auto wait_time_ms = duration_cast<milliseconds>(wait_time).count();
        if (wait_time_ms > 0)
            timeout_ms = wait_time_ms < timeout_ms ? timeout_ms - wait_time_ms : 0;

        m_event_queue.pop_front();
    }

    return S_OK;
}

HRESULT DX11Surface::wait_gpu_event_gpu(ID3D11DeviceContext* context) {
    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context is nullptr\n");
        return E_INVALIDARG;
    }

    std::unique_lock lk(m_event_queue_lock);
    // clear signalled events
    m_event_queue.flush();

    // if no pending events - return immediately
    if (m_event_queue.empty())
        return S_OK;

    HRESULT result = S_OK;

    // get parent device
    CComPtr<ID3D11Multithread> context_lock;
    result = context->QueryInterface<ID3D11Multithread>(&context_lock);
    if (FAILED(result) || context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    if (device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_FAIL;
    }

    CComPtr<ID3D11Device5> device5;
    result = device->QueryInterface<ID3D11Device5>(&device5);
    if (FAILED(result) || device5 == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    auto& gpu_event = m_event_queue.back();
    auto shared_fence = gpu_event.shared_fence;
    auto shared_fence_value = gpu_event.fence_value;

    // open shared fence
    CComPtr<ID3D11Fence> fence;
    result = device5->OpenSharedFence(shared_fence, __uuidof(ID3D11Fence), reinterpret_cast<void**>(&fence));
    if (FAILED(result) || fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->OpenSharedResource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D11DeviceContext4> context4;
    result = context->QueryInterface<ID3D11DeviceContext4>(&context4);
    if (FAILED(result) || context4 == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->QueryInterface failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // signal new event with updated fence reference
    result = signal_gpu_event(fence, shared_fence, shared_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": signal_gpu_event failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait for shared fence
    context_lock->Enter();
    result = context4->Wait(fence, shared_fence_value);
    context_lock->Leave();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->Wait4 failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT DX11Surface::wait_gpu_event_gpu(ID3D12CommandQueue* queue) {
    if (queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context is nullptr\n");
        return E_INVALIDARG;
    }

    std::unique_lock lk(m_event_queue_lock);
    // clear signalled events
    m_event_queue.flush();

    // if no pending events - return immediately
    if (m_event_queue.empty())
        return S_OK;

    HRESULT result = S_OK;

    // get parent device
    CComPtr<ID3D12Device> device;
    result = queue->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device));
    if (FAILED(result) || device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandQueue->GetDevice failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    auto& gpu_event = m_event_queue.back();
    auto shared_fence = gpu_event.shared_fence;
    auto shared_fence_value = gpu_event.fence_value;

    // open shared fence
    CComPtr<ID3D12Fence> fence;
    result = device->OpenSharedHandle(shared_fence, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&fence));
    if (FAILED(result) || fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->OpenSharedResource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // signal new event with updated fence reference
    result = signal_gpu_event(fence, shared_fence, shared_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": signal_gpu_event failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait for shared fence
    result = queue->Wait(fence, shared_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandQueue->Wait failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

LUID DX11Surface::get_device_luid() {
    return utils::get_adapter_luid_from_device(m_device);
}

UINT DX11Surface::get_width() {
    return m_desc.Width;
}

UINT DX11Surface::get_height() {
    return m_desc.Height;
}

DXGI_FORMAT DX11Surface::get_format() {
    return m_desc.Format;
}
