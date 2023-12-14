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

#include "desktop-duplicator.h"

#include "dx-utils.h"
#include "dx11-surface-pool.h"

#include <chrono>

DesktopDuplicator::~DesktopDuplicator() {
    stop();

    if (m_copy_fence_shared_handle)
        CloseHandle(m_copy_fence_shared_handle);
}

std::unique_ptr<DesktopDuplicator> DesktopDuplicator::create(const std::wstring& display_device_name) {

    auto instance = std::unique_ptr<DesktopDuplicator>(new DesktopDuplicator);

    HRESULT result = S_OK;
    
    // get adapter and output from display device name
    CComPtr<IDXGIAdapter> adapter;
    CComPtr<IDXGIOutput> output;
    result = utils::enum_adapter_by_display_name(&adapter, &output, display_device_name);
    if (result == DXGI_ERROR_NOT_FOUND) {
        // if not found try primary display
        result = utils::enum_primary_display(&adapter, &output);
        if (FAILED(result) || adapter == nullptr || output == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_primary_display failed, result = 0x%08x\n", result);
            return nullptr;
        }

        ga_logger(Severity::WARNING, __FUNCTION__ ": display device = %S is not found - using primary display\n", display_device_name.c_str());
    } else if (FAILED(result) || adapter == nullptr || output == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_adapter_by_display_name failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // query required interface
    CComPtr<IDXGIOutput1> output1;
    result = output->QueryInterface<IDXGIOutput1>(&output1);
    if (FAILED(result) || output1 == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutput1->QueryInterface failed, result = 0x%08x\n", result);
        return nullptr;
    }

    instance->m_adapter = adapter;
    instance->m_output = output1;

    // query adapter/output desc
    DXGI_ADAPTER_DESC adapter_desc = {};
    result = adapter->GetDesc(&adapter_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->GetDesc failed, result = 0x%08x\n", result);
        return nullptr;
    }

    DXGI_OUTPUT_DESC output_desc = {};
    result = output->GetDesc(&output_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutput->GetDesc failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // log adapter/ouput pair
    ga_logger(Severity::INFO, __FUNCTION__ ": found output with device name = %S, parent adapter = %S, LUID = 0x%x:0x%x\n",
        output_desc.DeviceName, adapter_desc.Description,
        adapter_desc.AdapterLuid.HighPart,
        adapter_desc.AdapterLuid.LowPart);

    instance->m_adapter_desc = adapter_desc;
    instance->m_output_desc = output_desc;

    // create duplication device
    result = utils::create_d3d11_device(adapter, &instance->m_device, &instance->m_device_context, &instance->m_device_context_lock);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::create_d3d11_device failed, result = 0x%08x\n", result);
        return nullptr;
    }

    if (instance->m_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return nullptr;
    }

    // create fence
    result = instance->m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, 
        __uuidof(ID3D11Fence), reinterpret_cast<void**>(&instance->m_copy_fence));
    if (FAILED(result) || instance->m_copy_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device5->CreateFence failed, result = 0x%08x\n", result);
        return nullptr;
    }

    result = instance->m_copy_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &instance->m_copy_fence_shared_handle);
    if (FAILED(result) || instance->m_copy_fence_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Fence->CreateSharedHandle failed, result = 0x%08x\n", result);
        return nullptr;
    }

    return instance;
}

HRESULT DesktopDuplicator::start() {
    // start capture thread
    m_keep_alive = 1;
    m_thread = std::move(std::thread(DesktopDuplicator::thread_proc, this));
    return S_OK;
}

void DesktopDuplicator::stop() {
    // stop capture thread
    m_keep_alive = 0;
    if (m_thread.joinable())
        m_thread.join();
}

HRESULT DesktopDuplicator::thread_proc(DesktopDuplicator* context) {
    using namespace std::chrono_literals;
    using namespace std::chrono;

    struct LogThreadLifetime {
        LogThreadLifetime() { ga_logger(Severity::INFO, "DesktopDuplicator thread started\n"); }
        ~LogThreadLifetime() { ga_logger(Severity::INFO, "DesktopDuplicator thread stoped\n"); }
    } log_thread_lifetime;

    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    HRESULT result = S_OK;

    const auto reset_retry_timeout = milliseconds(500);
    const auto acq_frame_timeout = milliseconds(500);

    bool reset_required = true;
    int reset_attempt_count = 0;
    constexpr int max_reset_attempts = 20;

    // capture loop
    while (context->keep_alive()) {
        // check if reset is required
        if (reset_required) {
            // max reset attempts reached - exit
            if (reset_attempt_count > max_reset_attempts)
                break;

            // yield thread to allow DWM transition
            auto awake_time = steady_clock::now() + reset_retry_timeout;
            std::this_thread::sleep_until(awake_time);

            // perform reset
            result = context->reset();
            if (FAILED(result)) {
                // reset failed - try again
                ga_logger(Severity::ERR, __FUNCTION__ ": DesktopDuplicator->reset failed, result = 0x%08x\n", result);
                reset_attempt_count++;
                continue;
            } else {
                // reset succeeded
                reset_required = false;
                reset_attempt_count = 0;
            }
        }

        // request new frame
        const UINT acq_frame_timeout_ms = duration_cast<milliseconds>(acq_frame_timeout).count();
        result = context->acquire_surface(acq_frame_timeout_ms);
        if (result == DXGI_ERROR_WAIT_TIMEOUT) {
            continue; // timeout, no new frame - try again
        } else if (FAILED(result)) {
            reset_required = true;
            continue; // try reset
        }

        // stage copy
        result = context->copy_surface();
        if (FAILED(result)) {
            // other error - try reset
            reset_required = true;
            continue;
        }

        // release frame
        result = context->release_surface();
        if (FAILED(result)) {
            // output mode change - reset
            reset_required = true;
            continue;
        }
    }

    return S_OK;
}

HRESULT DesktopDuplicator::reset() {
    // reset duplication objects
    m_duplication = nullptr;
    m_duplication_desc = {};

    if (m_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_FAIL;
    }
    if (m_output == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": output is nullptr\n");
        return E_FAIL;
    }

    CComPtr<IDXGIOutputDuplication> duplication;
    HRESULT result = m_output->DuplicateOutput(m_device, &duplication);
    if (FAILED(result) || duplication == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutput1->DuplicateOutput failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    DXGI_OUTDUPL_DESC duplication_desc = {};
    duplication->GetDesc(&duplication_desc);

    m_duplication = duplication;
    m_duplication_desc = duplication_desc;

    return S_OK;
}

HRESULT DesktopDuplicator::acquire_surface(UINT timeout_ms) {
    if (m_duplication == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": duplication object is nullptr\n");
        return E_FAIL;
    }

    // acquire next frame
    CComPtr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info = {};

    HRESULT result = m_duplication->AcquireNextFrame(timeout_ms, &frame_info, &resource);
    if (result == DXGI_ERROR_ACCESS_LOST)
        return DXGI_ERROR_ACCESS_LOST; // mode changed, reset needed
    else if (result == DXGI_ERROR_WAIT_TIMEOUT)
        return DXGI_ERROR_WAIT_TIMEOUT; // timeout
    else if (FAILED(result) || resource == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutputDuplication->AcquireNextFrame failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // query d3d11 texture interface
    CComPtr<ID3D11Texture2D> texture;
    result = resource->QueryInterface<ID3D11Texture2D>(&texture);
    if (FAILED(result) || texture == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Texture2D->QueryInterface failed, result = 0x%08x\n", result);
        std::ignore = m_duplication->ReleaseFrame();
        return E_FAIL;
    }

    // update input texture
    m_desktop_texture = texture;

    // update cursor
    const auto cursor_position_changed = update_cursor_position(frame_info);
    const auto cursor_shape_changed = update_cursor_shape(frame_info);

    if (cursor_position_changed || cursor_shape_changed) {
        std::unique_lock lk(m_acquire_cursor_lock);
        m_cursor_updated = true;
        m_output_cursor = m_cursor_state;
        lk.unlock();
        m_acquire_cursor_cv.notify_one();
    }

    return S_OK;
}

HRESULT DesktopDuplicator::copy_surface() {
    if (m_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (m_device_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (m_device_context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context lock is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    CComPtr<ID3D11Texture2D> src = m_desktop_texture;
    if (src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src texture is nullptr\n");
        return E_FAIL;
    }

    // capture submit start time
    using namespace std::chrono;
    auto capture_start_ts = clock_t::now();

    HRESULT result = S_OK;

    D3D11_TEXTURE2D_DESC src_desc = {};
    src->GetDesc(&src_desc);

    // reset dst surface pool if needed
    bool reset_surface_pool = (m_surface_pool == nullptr);
    if (m_surface_pool != nullptr) {
        auto dst_desc = m_surface_pool->get_texture_desc();
        reset_surface_pool = src_desc.Width != dst_desc.Width
            || src_desc.Height != dst_desc.Height
            || src_desc.Format != dst_desc.Format;
    }
    if (reset_surface_pool) {
        // create surface pool
        DX11SurfacePool::Desc pool_desc = {};
        pool_desc.device = m_device;
        pool_desc.texture_desc.Width = src_desc.Width;
        pool_desc.texture_desc.Height = src_desc.Height;
        pool_desc.texture_desc.MipLevels = 1;
        pool_desc.texture_desc.ArraySize = 1;
        pool_desc.texture_desc.Format = src_desc.Format;
        pool_desc.texture_desc.SampleDesc.Count = 1;
        pool_desc.texture_desc.SampleDesc.Quality = 0;
        pool_desc.texture_desc.Usage = D3D11_USAGE_DEFAULT;
        pool_desc.texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        pool_desc.texture_desc.CPUAccessFlags = 0; // no cpu access
        pool_desc.texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

        m_surface_pool = DX11SurfacePool::create(pool_desc);
        if (m_surface_pool == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": failed to create surface pool, result = 0x%08x\n", result);
            return E_FAIL;
        }
    }

    // acquire dst surface
    auto dst_surface = m_surface_pool->acquire();
    if (dst_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": DX12SurfacePool->acquire failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D11Texture2D> dst;
    result = dst_surface->open_shared_texture(m_device, &dst);
    if (FAILED(result) || dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": DX12Surface->open_shared_texture failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // stage copy
    m_device_context_lock->Enter();
    m_device_context->CopyResource(dst, src);
    m_device_context->Flush();

    // signal gpu fence
    auto fence_value = InterlockedIncrement(&m_copy_fence_value);
    result = m_device_context->Signal(m_copy_fence, fence_value);
    m_device_context_lock->Leave();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext4->Signal failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = dst_surface->signal_gpu_event(m_copy_fence, m_copy_fence_shared_handle, fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->signal_gpu_fence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // capture submit end time
    auto capture_end_ts = clock_t::now();

    // update capture surface
    std::unique_lock lk(m_acquire_frame_lock);
    m_output_frame = Frame::create(std::move(dst_surface), m_surface_pool);
    if (m_output_frame) {
        auto& timing_info = m_output_frame->get_timing_info();
        timing_info.capture_start_ts = capture_start_ts;
        timing_info.capture_end_ts = capture_end_ts;
        // use capture timestamp for presentation
        timing_info.presentation_ts = timing_info.capture_start_ts;
    }
    lk.unlock();
    m_acquire_frame_cv.notify_one();

    return S_OK;
}

HRESULT DesktopDuplicator::release_surface() {
    m_desktop_texture = nullptr;

    HRESULT result = m_duplication->ReleaseFrame();
    if (result == DXGI_ERROR_ACCESS_LOST) {
        return DXGI_ERROR_ACCESS_LOST; // mode changed, reset needed
    } else if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIOutputDuplication->ReleaseFrame failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

bool DesktopDuplicator::update_cursor_position(const DXGI_OUTDUPL_FRAME_INFO& frame_info) {
    if (frame_info.LastMouseUpdateTime.QuadPart == 0)
        return false; // no update

    const bool prev_visible = m_cursor_state.visible;
    const POINT prev_position = { m_cursor_state.x, m_cursor_state.y };

    const bool next_visible = frame_info.PointerPosition.Visible;
    const auto& next_position = frame_info.PointerPosition.Position;

    // visibility changed
    if (prev_visible != next_visible) {
        m_cursor_state.visible = next_visible;
        if (next_visible) {
            m_cursor_state.x = next_position.x;
            m_cursor_state.y = next_position.y;
        }
        return true;
    }

    // position changed
    if (next_visible) {
        if (prev_position.x != next_position.x || prev_position.y != next_position.y) {
            m_cursor_state.x = next_position.x;
            m_cursor_state.y = next_position.y;
            return true;
        }
    }

    return false; // no update
}

bool DesktopDuplicator::update_cursor_shape(const DXGI_OUTDUPL_FRAME_INFO& frame_info) {
    if (m_duplication == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": duplication object is nullptr\n");
        return false; // no update
    }

    const UINT buffer_size = frame_info.PointerShapeBufferSize;
    if (buffer_size == 0)
        return false; // no update

    if (m_shape_buffer.size() < buffer_size) {
        m_shape_buffer.resize(buffer_size);
    }

    UINT required_size = 0;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info = {};
    HRESULT result = m_duplication->GetFramePointerShape(buffer_size, m_shape_buffer.data(), &required_size, &shape_info);
    if (FAILED(result)) {
        ga_logger(Severity::DBG, __FUNCTION__ ": IDXGIOutputDuplication->GetFramePointerShape failed, result = 0x%08x\n", result);
        return false;
    }

    switch (shape_info.Type) {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        update_cursor_shape_monochrome(m_cursor_state, shape_info, m_shape_buffer);
        break;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        update_cursor_shape_color(m_cursor_state, shape_info, m_shape_buffer);
        break;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        update_cursor_shape_masked_color(m_cursor_state, shape_info, m_shape_buffer);
        break;
    default:
        ga_logger(Severity::ERR, __FUNCTION__ ": unexpected cursor shape type\n");
        return false;
    };

    return true;
}

HRESULT DesktopDuplicator::update_cursor_shape_monochrome(CursorState& state,
    const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data) {

    // check args
    const uint32_t expected_in_data_size = shape_info.Pitch * shape_info.Height;
    if (shape_data.size() < expected_in_data_size)
        return E_INVALIDARG;

    const uint32_t width = shape_info.Width;
    const uint32_t height = shape_info.Height / 2;
    const uint32_t pitch = shape_info.Width * 4;

    state.shape_present = true;
    state.shape_width = width;
    state.shape_height = height;
    state.shape_pitch = pitch;
    state.shape_hotspot_x = shape_info.HotSpot.x;
    state.shape_hotspot_y = shape_info.HotSpot.y;

    state.shape_data.resize(pitch * height);
    state.shape_xor_data.resize(pitch * height);

    const uint32_t xor_mask_offset = shape_info.Pitch * shape_info.Height / 2;
    const uint8_t* src_and = shape_data.data();
    const uint8_t* src_xor = shape_data.data() + xor_mask_offset;
    const uint32_t src_pitch = shape_info.Pitch;

    uint8_t* dst_and = state.shape_data.data();
    uint8_t* dst_xor = state.shape_xor_data.data();
    const uint32_t dst_pitch = pitch;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t bit_off = (x / 8) + y * src_pitch;
            uint32_t bit_shr = x & 7;
            uint32_t aa = ((src_and[bit_off] >> bit_shr) & 1);
            uint32_t xx = ((src_xor[bit_off] >> bit_shr) & 1);
            uint32_t dst_off = 4 * x + y * dst_pitch;

            if (aa == 0 && xx == 0) {
                // solid black
                dst_and[dst_off + 0] = 0;
                dst_and[dst_off + 1] = 0;
                dst_and[dst_off + 2] = 0;
                dst_and[dst_off + 3] = 0xFF;
                // transparent
                dst_xor[dst_off + 0] = 0;
                dst_xor[dst_off + 1] = 0;
                dst_xor[dst_off + 2] = 0;
                dst_xor[dst_off + 3] = 0;
            } else if (aa == 0 && xx == 1) {
                // solid white
                dst_and[dst_off + 0] = 0xFF;
                dst_and[dst_off + 1] = 0xFF;
                dst_and[dst_off + 2] = 0xFF;
                dst_and[dst_off + 3] = 0xFF;
                // transparent
                dst_xor[dst_off + 0] = 0;
                dst_xor[dst_off + 1] = 0;
                dst_xor[dst_off + 2] = 0;
                dst_xor[dst_off + 3] = 0;
            } else if (aa == 1 && xx == 0) {
                // transparent
                dst_and[dst_off + 0] = 0;
                dst_and[dst_off + 1] = 0;
                dst_and[dst_off + 2] = 0;
                dst_and[dst_off + 3] = 0;
                // transparent
                dst_xor[dst_off + 0] = 0;
                dst_xor[dst_off + 1] = 0;
                dst_xor[dst_off + 2] = 0;
                dst_xor[dst_off + 3] = 0;
            } else /* (aa == 1 && xx == 1) */ {
                // transparent
                dst_and[dst_off + 0] = 0;
                dst_and[dst_off + 1] = 0;
                dst_and[dst_off + 2] = 0;
                dst_and[dst_off + 3] = 0;
                // solid white - invert color
                dst_xor[dst_off + 0] = 0xFF;
                dst_xor[dst_off + 1] = 0xFF;
                dst_xor[dst_off + 2] = 0xFF;
                dst_xor[dst_off + 3] = 0xFF;
            }
        }
    }

    return S_OK;
}

HRESULT DesktopDuplicator::update_cursor_shape_color(CursorState& state,
    const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data) {
    // check args
    const uint32_t expected_in_data_size = shape_info.Pitch * shape_info.Height;
    if (shape_data.size() < expected_in_data_size)
        return E_INVALIDARG;

    const uint32_t width = shape_info.Width;
    const uint32_t height = shape_info.Height;
    const uint32_t pitch = shape_info.Pitch;

    state.shape_present = true;
    state.shape_width = width;
    state.shape_height = height;
    state.shape_pitch = pitch;
    state.shape_hotspot_x = shape_info.HotSpot.x;
    state.shape_hotspot_y = shape_info.HotSpot.y;

    const uint32_t shape_size = pitch * height;
    state.shape_data.resize(shape_size);
    state.shape_xor_data.clear();
    state.shape_data.assign(shape_data.begin(), shape_data.begin() + shape_size);

    return S_OK;
}

HRESULT DesktopDuplicator::update_cursor_shape_masked_color(CursorState& state,
    const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data) {
    // check args
    const uint32_t expected_in_data_size = shape_info.Pitch * shape_info.Height;
    if (shape_data.size() < expected_in_data_size)
        return E_INVALIDARG;

    const uint32_t width = shape_info.Width;
    const uint32_t height = shape_info.Height;
    const uint32_t pitch = shape_info.Pitch;

    state.shape_present = true;
    state.shape_width = width;
    state.shape_height = height;
    state.shape_pitch = pitch;
    state.shape_hotspot_x = shape_info.HotSpot.x;
    state.shape_hotspot_y = shape_info.HotSpot.y;

    const uint32_t shape_size = pitch * height;
    state.shape_data.resize(shape_size);
    state.shape_xor_data.resize(shape_size);
    state.shape_data.assign(shape_data.begin(), shape_data.begin() + shape_size);
    state.shape_xor_data.assign(shape_data.begin(), shape_data.begin() + shape_size);

    // alpha bits are xor mask - fix alpha channel
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t off = (4 * x) + 3 + (y * pitch);
            state.shape_data[off] = shape_data[off] ? 0 : 0xFF;
            state.shape_xor_data[off] = shape_data[off] ? 0xFF : 0;
        }
    }

    return S_OK;
}

HRESULT DesktopDuplicator::receive_frame(std::shared_ptr<Frame>& frame, UINT timeout_ms) {
    // wait for a new frame
    std::unique_lock lk(m_acquire_frame_lock);
    auto timeout = std::chrono::milliseconds(timeout_ms);
    auto signalled = m_acquire_frame_cv.wait_for(lk, timeout, [&]() -> bool { return m_output_frame != nullptr; });
    if (!signalled)
        return DXGI_ERROR_WAIT_TIMEOUT;

    // assign new frame
    frame = m_output_frame;
    m_output_frame.reset();
    return S_OK;
}

HRESULT DesktopDuplicator::receive_cursor(CursorState& cursor_state, UINT timeout_ms) {
    // wait for cursor update
    std::unique_lock lk(m_acquire_cursor_lock);
    // if timeout is 0 - return state immediately
    if (timeout_ms == 0) {
        cursor_state = m_output_cursor;
        return S_OK;
    }

    auto timeout = std::chrono::milliseconds(timeout_ms);
    auto signalled = m_acquire_cursor_cv.wait_for(lk, timeout, [&]() -> bool { 
        return m_cursor_updated;
    });
    if (!signalled)
        return DXGI_ERROR_WAIT_TIMEOUT;

    // assign cursor state
    cursor_state = m_output_cursor;
    m_cursor_updated = false;
    return S_OK;
}
