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

#include "video-processor.h"

#include "dx-utils.h"
#include "dx12-surface-pool.h"
#include "dx12-surface.h"
#include "frame-provider.h"

#include <dxgi1_4.h>

VideoProcessor::~VideoProcessor() {
    stop();

    if (m_src_copy_fence_shared_handle)
        CloseHandle(m_src_copy_fence_shared_handle);
    if (m_dst_copy_fence_shared_handle)
        CloseHandle(m_dst_copy_fence_shared_handle);
    if (m_vp_fence_shared_handle)
        CloseHandle(m_vp_fence_shared_handle);

    if (m_src_copy_event)
        CloseHandle(m_src_copy_event);
    if (m_dst_copy_event)
        CloseHandle(m_dst_copy_event);
    if (m_vp_event)
        CloseHandle(m_vp_event);
}

std::unique_ptr<VideoProcessor> VideoProcessor::create(const VideoProcessor::Desc& desc) {
    // validate params
    HRESULT result = validate_video_processor_desc(desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": validate_video_processor_desc failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // get target adapter
    CComPtr<IDXGIAdapter> adapter;
    result = utils::enum_adapter_by_luid(&adapter, desc.adapter_luid);
    if (FAILED(result) || adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_adapter_by_luid failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // create instance
    auto instance = std::unique_ptr<VideoProcessor>(new VideoProcessor);

    // create video processing device
    CComPtr<ID3D12Device> device;
    result = utils::create_d3d12_device(adapter, &device);
    if (FAILED(result) || device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::create_d3d12_device failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_device = device;
    instance->m_dst_device_luid = utils::get_adapter_luid_from_device(device);

    // create video device
    CComPtr<ID3D12VideoDevice> video_device;
    result = device->QueryInterface<ID3D12VideoDevice>(&video_device);
    if (FAILED(result) || video_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->QueryInterface failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_video_device = video_device;

    // create video command allocator
    CComPtr<ID3D12CommandAllocator> video_cmd_alloc;
    result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, 
        __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&video_cmd_alloc));
    if (FAILED(result) || video_cmd_alloc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandAllocator failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_video_cmd_alloc = video_cmd_alloc;

    // create video command queue
    D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
    command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    CComPtr<ID3D12CommandQueue> video_cmd_queue;
    result = instance->m_device->CreateCommandQueue(&command_queue_desc, 
        __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&video_cmd_queue));
    if (FAILED(result) || video_cmd_queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandQueue failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_video_cmd_queue = video_cmd_queue;

    // create video command list
    CComPtr<ID3D12VideoProcessCommandList> video_cmd_list;
    result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, 
        video_cmd_alloc, nullptr /* init_state */, 
        __uuidof(ID3D12VideoProcessCommandList), reinterpret_cast<void**>(&video_cmd_list));
    if (FAILED(result) || video_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandList failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_video_cmd_list = video_cmd_list;

    // close video command list
    result = video_cmd_list->Close();
    if (FAILED(result) || video_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12VideoProcessCommandList->Close failed, result = 0x%08x\n", result);
        return nullptr;
    }

    CComPtr<ID3D12Fence> fence;
    result = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,
        __uuidof(ID3D12Fence), reinterpret_cast<void**>(&fence));
    if (FAILED(result) || fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateFence failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_vp_fence = fence;
    instance->m_vp_fence_value = 0;

    HANDLE shared_fence = nullptr;
    result = device->CreateSharedHandle(fence, nullptr, GENERIC_ALL, nullptr, &shared_fence);
    if (FAILED(result) || shared_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateSharedHandle failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_vp_fence_shared_handle = shared_fence;

    // create events
    instance->m_src_copy_event = CreateEvent(nullptr, false, false, nullptr);
    if (instance->m_src_copy_event == nullptr) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateEvent failed, result = 0x%08x\n", result);
        return nullptr;
    }

    instance->m_dst_copy_event = CreateEvent(nullptr, false, false, nullptr);
    if (instance->m_dst_copy_event == nullptr) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateEvent failed, result = 0x%08x\n", result);
        return nullptr;
    }

    instance->m_vp_event = CreateEvent(nullptr, false, false, nullptr);
    if (instance->m_vp_event == nullptr) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateEvent failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // fill params
    instance->m_frame_rate = desc.frame_rate;
    instance->m_output_format = desc.output_format;

    using namespace std::chrono;
    using namespace std::chrono_literals;

    DXGI_RATIONAL tick_rate = {};
    tick_rate.Numerator = 1;
    tick_rate.Denominator = instance->m_frame_rate;
    instance->m_frame_interval = duration_t(duration_cast<duration_t>(1s).count() * tick_rate.Numerator / tick_rate.Denominator);

    return instance;
}

HRESULT VideoProcessor::start() {
    if (m_frame_provider == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": frame provider is nullptr\n");
        return E_FAIL;
    }

    m_keep_alive = 1;
    m_processing_thread = std::move(std::thread(VideoProcessor::processing_thread_proc, this));

    return S_OK;
}

void VideoProcessor::stop() {
    m_keep_alive = 0;
    if (m_processing_thread.joinable())
        m_processing_thread.join();
}

HRESULT VideoProcessor::validate_video_processor_desc(const VideoProcessor::Desc& desc) {
    // video processor device LUID
    CComPtr<IDXGIAdapter> adapter;
    HRESULT result = utils::enum_adapter_by_luid(&adapter, desc.adapter_luid);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid adapter luid\n");
        return E_FAIL;
    }

    // video processor output frame rate
    if (desc.frame_rate == 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": output frame rate should be greater 0\n");
        return E_FAIL;
    }

    // frame output format
    if (desc.output_format == DXGI_FORMAT_UNKNOWN) {
        ga_logger(Severity::ERR, __FUNCTION__ ": output frame format is unset\n");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT VideoProcessor::receive_frame(std::shared_ptr<Frame>& frame, UINT timeout_ms) {
    // wait for a new frame
    std::unique_lock lk(m_output_lock);
    auto timeout = std::chrono::milliseconds(timeout_ms);
    auto signalled = m_output_cv.wait_for(lk, timeout, [&]() -> bool { return m_output_frame != nullptr; });
    if (!signalled)
        return DXGI_ERROR_WAIT_TIMEOUT;

    // assign new frame
    frame = m_output_frame;
    m_output_frame.reset();
    return S_OK;
}

HRESULT VideoProcessor::register_frame_provider(std::shared_ptr<FrameProvider> frame_provider) {
    if (frame_provider == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    
    m_frame_provider = frame_provider;
    return S_OK;
}

void VideoProcessor::update_input_frame(std::shared_ptr<Frame> frame) {
    if (frame == nullptr)
        return;

    std::lock_guard lk(m_input_lock);
    m_input_frame = frame;
}

class FrameTimeEstimator {
public:
    using duration_t = std::chrono::steady_clock::duration;

    explicit FrameTimeEstimator(duration_t interval, uint32_t max_size) : m_interval(interval), m_max_size(max_size) {}

    void push(duration_t next) {
        if (m_ring_buffer.size() < m_max_size) {
            m_ring_buffer.push_back(next);
        } else {
            if (m_pos >= m_ring_buffer.size())
                m_pos = 0;
            m_ring_buffer[m_pos++] = next;
        }
    }

    void clear() { m_ring_buffer.clear(); }

    duration_t next() const { 
        duration_t avg = average();
        duration_t est = 2 * m_interval - average();
        return est;
    }

    duration_t average() const {
        if (m_ring_buffer.empty())
            return m_interval;
        duration_t sum(0);
        for (auto& v : m_ring_buffer)
            sum += v;
        duration_t avg = sum / m_ring_buffer.size();
        return avg;
    }

private:
    std::vector<duration_t> m_ring_buffer;
    uint32_t m_max_size = 1;
    duration_t m_interval;
    uint32_t m_pos = 0;
};

HRESULT VideoProcessor::processing_thread_proc(VideoProcessor* context) {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    struct LogThreadLifetime {
        LogThreadLifetime() { ga_logger(Severity::INFO, "VideoProcessor processing thread started\n"); }
        ~LogThreadLifetime() { ga_logger(Severity::INFO, "VideoProcessor processing thread stoped\n"); }
    } log_thread_lifetime;

    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    auto frame_provider = context->get_frame_provider();
    if (frame_provider == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": frame provider is nullptr\n");
        return E_FAIL;
    }

    auto frame_interval = context->m_frame_interval;
    FrameTimeEstimator fte(frame_interval, 10);

    auto prev_frame_ts = steady_clock::now();
    while (context->keep_alive()) {
        // try to guess next frame time
        auto estimated_frame_time = fte.next();

        std::shared_ptr<Frame> frame;

        // capture frame
        const UINT capture_timeout_ms = 4; // cap input frame rate to 250fps = 4ms
        auto capture_start = steady_clock::now();
        HRESULT result = frame_provider->receive_frame(frame, capture_timeout_ms);
        auto capture_end = steady_clock::now();
        auto capture_time = capture_end - capture_start;

        // process frame
        auto proc_start = steady_clock::now();
        if (SUCCEEDED(result) && frame != nullptr) {
            // new frame received - update vp input
            context->update_input_frame(frame);
            // new frame received - process
            result = context->process_frame();
            if (FAILED(result)) {
                ga_logger(Severity::ERR, __FUNCTION__ ": VideoProcessor->process_frame failed, result = 0x%08x\n", result);
                continue;
            }
        }
        auto proc_end = steady_clock::now();
        auto proc_time = proc_end - proc_start;

        // presentation timestamp
        auto frame_ts = steady_clock::now();
        // update output frame
        context->update_output_frame();
        // current frame time
        auto frame_time = frame_ts - prev_frame_ts;
        // update frame stats
        prev_frame_ts = frame_ts;
        fte.push(frame_time);

        // frame rate control
        // delay next frame capture to match frame rate
        auto frc_start = steady_clock::now();
        auto frc_delay_ts = frame_ts + estimated_frame_time - proc_time - capture_time;
        if (frc_start < frc_delay_ts) {
            auto sleep_time = frc_delay_ts - frc_start;
            auto sleep_time_ms = duration_cast<milliseconds>(sleep_time);
            // sleep timer resolution is not precise enough - use ms here
            std::this_thread::sleep_for(sleep_time_ms);
            // small extra delay for better frame pacing
            while (steady_clock::now() < frc_delay_ts) { /* empty */ }
        }
        auto frc_end = steady_clock::now();
        auto frc_time = frc_end - frc_start;
    }

    return S_OK;
}

HRESULT VideoProcessor::reset_copy_processor(const LUID& src_device_luid, uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format) {
    // reset src to staging context
    m_src_copy_device = nullptr;
    m_src_copy_cmd_alloc = nullptr;
    m_src_copy_cmd_queue = nullptr;
    m_src_copy_cmd_list = nullptr;
    m_src_copy_fence = nullptr;
    if (m_src_copy_fence_shared_handle) {
        CloseHandle(m_src_copy_fence_shared_handle);
        m_src_copy_fence_shared_handle = nullptr;
    }

    // reset staging to dst context
    m_dst_copy_cmd_alloc = nullptr;
    m_dst_copy_cmd_queue = nullptr;
    m_dst_copy_cmd_list = nullptr;
    m_dst_copy_fence = nullptr;
    if (m_dst_copy_fence_shared_handle) {
        CloseHandle(m_dst_copy_fence_shared_handle);
        m_dst_copy_fence_shared_handle = nullptr;
    }

    // reset surface pools
    m_copy_staging_surface = nullptr;
    m_copy_dst_surface = nullptr;
    m_src_device_copy_src = nullptr;
    m_src_device_copy_dst = nullptr;
    m_dst_device_copy_src = nullptr;
    m_dst_device_copy_dst = nullptr;

    // same device - disable copy
    if (utils::is_same_luid(src_device_luid, m_dst_device_luid)) {
        m_src_device_luid = src_device_luid;
        m_cross_adapter_copy_needed = false;
        return S_OK;
    }

    // create src to staging context
    CComPtr<IDXGIAdapter> adapter;
    HRESULT result = utils::enum_adapter_by_luid(&adapter, src_device_luid);
    if (FAILED(result) || adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_adapter_by_luid failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D12Device> device;
    result = utils::create_d3d12_device(adapter, &device);
    if (FAILED(result) || device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::create_d3d12_device failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D12Device> src_device = device;
    CComPtr<ID3D12Device> dst_device = m_device;
    m_src_copy_device = src_device;

    CComPtr<ID3D12CommandAllocator> src_copy_cmd_alloc;
    result = src_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
        __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&src_copy_cmd_alloc));
    if (FAILED(result) || src_copy_cmd_alloc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandAllocator failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_cmd_alloc = src_copy_cmd_alloc;

    D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    CComPtr<ID3D12CommandQueue> src_copy_cmd_queue;
    result = src_device->CreateCommandQueue(&command_queue_desc,
        __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&src_copy_cmd_queue));
    if (FAILED(result) || src_copy_cmd_queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandQueue failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_cmd_queue = src_copy_cmd_queue;

    CComPtr<ID3D12GraphicsCommandList> src_copy_cmd_list;
    result = src_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
        src_copy_cmd_alloc, nullptr /* init_state */,
        __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&src_copy_cmd_list));
    if (FAILED(result) || src_copy_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandList failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    result = src_copy_cmd_list->Close();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->Close failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_cmd_list = src_copy_cmd_list;

    CComPtr<ID3D12Fence> src_copy_fence;
    result = src_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,
        __uuidof(ID3D12Fence), reinterpret_cast<void**>(&src_copy_fence));
    if (FAILED(result) || src_copy_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateFence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_fence = src_copy_fence;
    m_src_copy_fence_value = 0;

    HANDLE src_copy_fence_shared_handle = nullptr;
    result = src_device->CreateSharedHandle(src_copy_fence, nullptr, GENERIC_ALL, nullptr, &src_copy_fence_shared_handle);
    if (FAILED(result) || src_copy_fence_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateSharedHandle failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_fence_shared_handle = src_copy_fence_shared_handle;

    // create staging to dst context
    CComPtr<ID3D12CommandAllocator> dst_copy_cmd_alloc;
    result = dst_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
        __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&dst_copy_cmd_alloc));
    if (FAILED(result) || dst_copy_cmd_alloc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandAllocator failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_cmd_alloc = dst_copy_cmd_alloc;

    CComPtr<ID3D12CommandQueue> dst_copy_cmd_queue;
    result = dst_device->CreateCommandQueue(&command_queue_desc,
        __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&dst_copy_cmd_queue));
    if (FAILED(result) || dst_copy_cmd_queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandQueue failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_cmd_queue = dst_copy_cmd_queue;

    CComPtr<ID3D12GraphicsCommandList> dst_copy_cmd_list;
    result = dst_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
        dst_copy_cmd_alloc, nullptr /* init_state */,
        __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&dst_copy_cmd_list));
    if (FAILED(result) || dst_copy_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateCommandList failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    result = dst_copy_cmd_list->Close();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->Close failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_cmd_list = dst_copy_cmd_list;

    CComPtr<ID3D12Fence> dst_copy_fence;
    result = dst_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,
        __uuidof(ID3D12Fence), reinterpret_cast<void**>(&dst_copy_fence));
    if (FAILED(result) || dst_copy_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateFence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_fence = dst_copy_fence;
    m_dst_copy_fence_value = 0;

    HANDLE dst_copy_fence_shared_handle = nullptr;
    result = dst_device->CreateSharedHandle(dst_copy_fence, nullptr, GENERIC_ALL, nullptr, &dst_copy_fence_shared_handle);
    if (FAILED(result) || dst_copy_fence_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Device->CreateSharedHandle failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_fence_shared_handle = dst_copy_fence_shared_handle;

    // create resources
    std::unique_ptr<DX12Surface> copy_staging_surface;
    {
        D3D12_HEAP_PROPERTIES heap_props = {};
        heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resource_desc.Width = frame_width;
        resource_desc.Height = frame_height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = frame_format;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        copy_staging_surface = DX12Surface::create(src_device, &heap_props, heap_flags, &resource_desc);
        if (copy_staging_surface == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": DX12Surface->create failed\n");
            return E_FAIL;
        }
    }

    std::unique_ptr<DX12Surface> copy_dst_surface;
    {
        D3D12_HEAP_PROPERTIES heap_props = {};
        heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resource_desc.Width = frame_width;
        resource_desc.Height = frame_height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = frame_format;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        copy_dst_surface = DX12Surface::create(dst_device, &heap_props, heap_flags, &resource_desc);
        if (copy_dst_surface == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": DX12Surface->create failed\n");
            return E_FAIL;
        }
    }

    CComPtr<ID3D12Resource> src_device_copy_dst;
    result = copy_staging_surface->open_shared_resource(src_device, &src_device_copy_dst);
    if (FAILED(result) || src_device_copy_dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D12Resource> dst_device_copy_src;
    result = copy_staging_surface->open_shared_resource(dst_device, &dst_device_copy_src);
    if (FAILED(result) || dst_device_copy_src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D12Resource> dst_device_copy_dst;
    result = copy_dst_surface->open_shared_resource(dst_device, &dst_device_copy_dst);
    if (FAILED(result) || dst_device_copy_dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // device luid 
    m_src_device_luid = src_device_luid;

    // copy surfaces and refs
    m_copy_staging_surface = std::move(copy_staging_surface);
    m_copy_dst_surface = std::move(copy_dst_surface);
    m_src_device_copy_src = nullptr; // filled in on a new frame
    m_src_device_copy_dst = src_device_copy_dst;
    m_dst_device_copy_src = dst_device_copy_src;
    m_dst_device_copy_dst = dst_device_copy_dst;

    m_cross_adapter_copy_needed = true;

    return S_OK;
}

HRESULT VideoProcessor::reset_video_processor(uint32_t src_frame_width, uint32_t src_frame_height, DXGI_FORMAT src_frame_format) {
    if (m_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_FAIL;
    }
    if (m_video_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": video device is nullptr\n");
        return E_FAIL;
    }

    m_video_processor = nullptr;
    m_output_surface_pool = nullptr;

    // destination frame format
    uint32_t dst_frame_width = src_frame_width;
    uint32_t dst_frame_height = src_frame_height;
    DXGI_FORMAT dst_frame_format = m_output_format;

    // create output surface pool
    DX12SurfacePool::Desc pool_desc = {};
    pool_desc.device = m_device;
    pool_desc.heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    pool_desc.heap_flags = D3D12_HEAP_FLAG_SHARED;
    pool_desc.resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    pool_desc.resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    pool_desc.resource_desc.Width = dst_frame_width;
    pool_desc.resource_desc.Height = dst_frame_height;
    pool_desc.resource_desc.DepthOrArraySize = 1;
    pool_desc.resource_desc.MipLevels = 1;
    pool_desc.resource_desc.Format = dst_frame_format;
    pool_desc.resource_desc.SampleDesc.Count = 1;
    pool_desc.resource_desc.SampleDesc.Quality = 0;
    pool_desc.resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    pool_desc.resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    m_output_surface_pool = DX12SurfacePool::create(pool_desc);
    if (m_output_surface_pool == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": failed to create surface pool\n");
        return E_FAIL;
    }

    // create video processor 
    D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC in_stream_desc = {};
    in_stream_desc.Format = src_frame_format;
    in_stream_desc.ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    in_stream_desc.SourceAspectRatio.Numerator = src_frame_width;
    in_stream_desc.SourceAspectRatio.Denominator = src_frame_height;
    in_stream_desc.DestinationAspectRatio.Numerator = src_frame_width;
    in_stream_desc.DestinationAspectRatio.Denominator = src_frame_height;

    in_stream_desc.FrameRate.Numerator = 60;
    in_stream_desc.FrameRate.Denominator = 1;

    in_stream_desc.SourceSizeRange.MaxWidth = src_frame_width;
    in_stream_desc.SourceSizeRange.MaxHeight = src_frame_height;
    in_stream_desc.SourceSizeRange.MinWidth = src_frame_width;
    in_stream_desc.SourceSizeRange.MinHeight = src_frame_height;

    in_stream_desc.DestinationSizeRange.MaxWidth = src_frame_width;
    in_stream_desc.DestinationSizeRange.MaxHeight = src_frame_height;
    in_stream_desc.DestinationSizeRange.MinWidth = src_frame_width;
    in_stream_desc.DestinationSizeRange.MinHeight = src_frame_height;

    in_stream_desc.EnableOrientation = false;
    in_stream_desc.FilterFlags = D3D12_VIDEO_PROCESS_FILTER_FLAG_NONE;
    in_stream_desc.StereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
    in_stream_desc.FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
    in_stream_desc.DeinterlaceMode = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
    in_stream_desc.EnableAlphaBlending = false;
    in_stream_desc.EnableAutoProcessing = false;

    D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC out_stream_desc = {};
    out_stream_desc.Format = dst_frame_format;
    out_stream_desc.ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    out_stream_desc.AlphaFillMode = D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
    out_stream_desc.FrameRate.Numerator = 60;
    out_stream_desc.FrameRate.Denominator = 1;
    out_stream_desc.EnableStereo = false;

    CComPtr<ID3D12VideoProcessor> video_processor;
    HRESULT result = m_video_device->CreateVideoProcessor(
        1, &out_stream_desc, 1, &in_stream_desc, __uuidof(ID3D12VideoProcessor),
        reinterpret_cast<void**>(&video_processor));
    if (FAILED(result) || video_processor == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12VideoDevice->CreateVideoProcessor failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_video_processor = video_processor;
    m_vp_in_stream_desc = in_stream_desc;
    m_vp_out_stream_desc = out_stream_desc;

    return S_OK;
}

HRESULT VideoProcessor::copy_src_to_staging() {

    if (m_copy_src_frame == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src frame is nullptr\n");
        return E_FAIL;
    }

    Surface* src_surface = m_copy_src_frame->get_surface();
    Surface* dst_surface = m_copy_staging_surface.get();

    if (src_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src surface is nullptr\n");
        return E_FAIL;
    }
    if (dst_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": dst surface is nullptr\n");
        return E_FAIL;
    }

    // wait for previous copy op
    if (m_src_copy_event == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }
    if (m_src_copy_event_signalled) {
        auto wait_result = WaitForSingleObject(m_src_copy_event, m_gpu_fence_timeout);
        if (wait_result != WAIT_OBJECT_0) {
            ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
            return E_FAIL;
        }
        m_src_copy_event_signalled = false;
    }

    // reset src resource
    m_src_device_copy_src = nullptr;

    // open src surface on copy device
    HRESULT result = src_surface->open_shared_resource(m_src_copy_device, &m_src_device_copy_src);
    if (FAILED(result) || m_src_device_copy_src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D12Resource> src = m_src_device_copy_src;
    CComPtr<ID3D12Resource> dst = m_src_device_copy_dst;

    if (src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src resource is nullptr\n");
        return E_FAIL;
    }
    if (dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": dst resource is nullptr\n");
        return E_FAIL;
    }
    
    if (m_src_copy_cmd_alloc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command allocator is nullptr\n");
        return E_FAIL;
    }
    if (m_src_copy_cmd_queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command queue is nullptr\n");
        return E_FAIL;
    }
    if (m_src_copy_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command list is nullptr\n");
        return E_FAIL;
    }
    if (m_src_copy_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": fence is nullptr\n");
        return E_FAIL;
    }

    // reset command allocator
    result = m_src_copy_cmd_alloc->Reset();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandAllocator->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // fill command list
    result = m_src_copy_cmd_list->Reset(m_src_copy_cmd_alloc, nullptr);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // set resource state
    D3D12_RESOURCE_BARRIER states_before[2] = {};
    states_before[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[0].Transition.pResource = src;
    states_before[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    states_before[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[1].Transition.pResource = dst;
    states_before[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    m_src_copy_cmd_list->ResourceBarrier(2, states_before);

    // copy resource
    m_src_copy_cmd_list->CopyResource(dst, src);

    // set resource state
    D3D12_RESOURCE_BARRIER states_after[2] = {};
    states_after[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[0].Transition.pResource = src;
    states_after[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    states_after[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    states_after[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[1].Transition.pResource = dst;
    states_after[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    states_after[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    m_src_copy_cmd_list->ResourceBarrier(2, states_after);

    // close command list
    result = m_src_copy_cmd_list->Close();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->close failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait for src surface
    result = src_surface->wait_gpu_event_gpu(m_src_copy_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_fence_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait for dst surface
    result = dst_surface->wait_gpu_event_gpu(m_src_copy_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_fence_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // execute command list
    ID3D12CommandList* command_lists[] = { m_src_copy_cmd_list };
    m_src_copy_cmd_queue->ExecuteCommandLists(1, command_lists);

    // signal gpu fence
    auto fence_value = InterlockedIncrement(&m_src_copy_fence_value);
    result = m_src_copy_cmd_queue->Signal(m_src_copy_fence, m_src_copy_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandQueue->Signal failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = m_src_copy_fence->SetEventOnCompletion(m_src_copy_fence_value, m_src_copy_event);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Fence->SetEventOnCompletion failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_src_copy_event_signalled = true;

    result = dst_surface->signal_gpu_event(m_src_copy_fence, m_src_copy_fence_shared_handle, m_src_copy_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->signal_gpu_fence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT VideoProcessor::copy_staging_to_dst() {

    auto src_surface = m_copy_staging_surface.get();
    auto dst_surface = m_copy_dst_surface.get();

    CComPtr<ID3D12Resource> src = m_dst_device_copy_src;
    CComPtr<ID3D12Resource> dst = m_dst_device_copy_dst;

    if (src_surface == nullptr || src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src surface is nullptr\n");
        return E_FAIL;
    }
    if (dst_surface == nullptr || dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": dst surface is nullptr\n");
        return E_FAIL;
    }

    if (m_dst_copy_cmd_alloc == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command allocator is nullptr\n");
        return E_FAIL;
    }
    if (m_dst_copy_cmd_queue == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command queue is nullptr\n");
        return E_FAIL;
    }
    if (m_dst_copy_cmd_list == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": command list is nullptr\n");
        return E_FAIL;
    }
    if (m_dst_copy_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": fence is nullptr\n");
        return E_FAIL;
    }

    // wait for previous copy op
    if (m_dst_copy_event == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }
    if (m_dst_copy_event_signalled) {
        auto wait_result = WaitForSingleObject(m_dst_copy_event, m_gpu_fence_timeout);
        if (wait_result != WAIT_OBJECT_0) {
            ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
            return E_FAIL;
        }
        m_dst_copy_event_signalled = false;
    }

    // wait for previous video processing op
    if (m_vp_event == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }
    if (m_vp_event_signalled) {
        auto wait_result = WaitForSingleObject(m_vp_event, m_gpu_fence_timeout);
        if (wait_result != WAIT_OBJECT_0) {
            ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
            return E_FAIL;
        }
        m_vp_event_signalled = false;
    }

    // reset command allocator
    HRESULT result = m_dst_copy_cmd_alloc->Reset();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandAllocator->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // fill command list
    result = m_dst_copy_cmd_list->Reset(m_dst_copy_cmd_alloc, nullptr);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // set resource state
    D3D12_RESOURCE_BARRIER states_before[2] = {};
    states_before[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[0].Transition.pResource = src;
    states_before[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    states_before[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[1].Transition.pResource = dst;
    states_before[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    m_dst_copy_cmd_list->ResourceBarrier(2, states_before);

    // copy resource
    m_dst_copy_cmd_list->CopyResource(dst, src);

    // set resource state
    D3D12_RESOURCE_BARRIER states_after[2] = {};
    states_after[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[0].Transition.pResource = src;
    states_after[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    states_after[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    states_after[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[1].Transition.pResource = dst;
    states_after[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    states_after[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    m_dst_copy_cmd_list->ResourceBarrier(2, states_after);

    // close command list
    result = m_dst_copy_cmd_list->Close();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12GraphicsCommandList->close failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait src surface
    result = src_surface->wait_gpu_event_gpu(m_dst_copy_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_fence_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait dst surface
    result = dst_surface->wait_gpu_event_gpu(m_dst_copy_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_fence_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // execute command list
    ID3D12CommandList* command_lists[] = { m_dst_copy_cmd_list };
    m_dst_copy_cmd_queue->ExecuteCommandLists(1, command_lists);

    // signal gpu fence
    auto fence_value = InterlockedIncrement(&m_dst_copy_fence_value);
    result = m_dst_copy_cmd_queue->Signal(m_dst_copy_fence, m_dst_copy_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandQueue->Signal failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = m_dst_copy_fence->SetEventOnCompletion(m_dst_copy_fence_value, m_dst_copy_event);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Fence->SetEventOnCompletion failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_dst_copy_event_signalled = true;

    result = dst_surface->signal_gpu_event(m_dst_copy_fence, m_dst_copy_fence_shared_handle, m_dst_copy_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->signal_gpu_fence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT VideoProcessor::process_frame() {
    if (m_device == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device is nullptr\n");
        return E_FAIL;
    }

#if 0 // [fixme] remove
    if (m_device_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": capture device context is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (m_device_context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": capture device context is invalid\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
#endif

    HRESULT result = S_OK;

    // get source frame
    std::unique_lock input_lk(m_input_lock);
    std::shared_ptr<Frame> src_frame = m_input_frame;
    input_lk.unlock();

    if (src_frame == nullptr)
        return S_OK; // nothing to process - return

    // get source surface
    Surface* src_surface = src_frame->get_surface();
    if (src_surface == nullptr)
        return S_OK; // nothing to process - return

    // check reset condition
    auto src_luid = src_surface->get_device_luid();
    const bool src_device_changed = !utils::is_same_luid(m_src_device_luid, src_luid);

    auto src_width = src_surface->get_width();
    auto src_height = src_surface->get_height();
    auto src_format = src_surface->get_format();
    const bool src_surface_changed =
        src_width != m_vp_in_stream_desc.SourceSizeRange.MaxWidth ||
        src_height != m_vp_in_stream_desc.SourceSizeRange.MaxHeight ||
        src_format != m_vp_in_stream_desc.Format;

    // reset video processor if needed
    const bool reset_required = src_device_changed || src_surface_changed;
    if (reset_required) {
        // reset copy processor
        HRESULT result = reset_copy_processor(src_luid, src_width, src_height, src_format);
        if (FAILED(result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": reset_copy_processor failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        // reset video processor
        result = reset_video_processor(src_width, src_height, src_format);
        if (FAILED(result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": reset_processor failed, result = 0x%08x\n", result);
            return E_FAIL;
        }
    }

    // cross-adapter copy
    if (m_cross_adapter_copy_needed) {
        // get staging surface
        m_copy_src_frame = src_frame;

        // copy src to staging
        HRESULT result = copy_src_to_staging();
        if (FAILED(result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": copy_src_to_staging failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        // copy staging to dst
        result = copy_staging_to_dst();
        if (FAILED(result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": copy_staging_to_dst failed, result = 0x%08x\n", result);
            return E_FAIL;
        }

        // replace src surface
        src_surface = m_copy_dst_surface.get();
        if (src_surface == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": src surface is nullptr\n");
            return E_FAIL;
        }
    }

    // wait for previous video processing op
    if (m_vp_event == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": event handle is nullptr\n");
        return E_FAIL;
    }
    if (m_vp_event_signalled) {
        auto wait_result = WaitForSingleObject(m_vp_event, m_gpu_fence_timeout);
        if (wait_result != WAIT_OBJECT_0) {
            ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
            return E_FAIL;
        }
        m_vp_event_signalled = false;
    }

    // reset src resource
    m_input_src = nullptr;

    // open src surface on video device
    result = src_surface->open_shared_resource(m_device, &m_input_src);
    if (FAILED(result) || m_input_src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    CComPtr<ID3D12Resource> src = m_input_src;

    // acquire dst surface
    if (m_output_surface_pool == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": output surface pool is nullptr\n");
        return E_FAIL;
    }

    auto dst_surface = m_output_surface_pool->acquire();
    if (FAILED(result) || dst_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": SurfacePool->acquire failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // open dst surface on video device
    CComPtr<ID3D12Resource> dst;
    result = dst_surface->open_shared_resource(m_device, &dst);
    if (FAILED(result) || dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_resource failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // reset command allocator
    result = m_video_cmd_alloc->Reset();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandAllocator->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // fill command list
    result = m_video_cmd_list->Reset(m_video_cmd_alloc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12VideoProcessCommandList->Reset failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // set resource state
    D3D12_RESOURCE_BARRIER states_before[2] = {};
    states_before[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[0].Transition.pResource = src;
    states_before[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ;

    states_before[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_before[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_before[1].Transition.pResource = dst;
    states_before[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_before[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    states_before[1].Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE;

    m_video_cmd_list->ResourceBarrier(2, states_before);

    // process frame
    D3D12_RESOURCE_DESC in_resource_desc = src->GetDesc();
    D3D12_RESOURCE_DESC out_resource_desc = dst->GetDesc();

    D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS in_stream_args = {};
    in_stream_args.InputStream[0].pTexture2D = src;
    in_stream_args.InputStream[0].Subresource = 0;

    in_stream_args.Transform.SourceRectangle.left = 0;
    in_stream_args.Transform.SourceRectangle.top = 0;
    in_stream_args.Transform.SourceRectangle.right = in_resource_desc.Width;
    in_stream_args.Transform.SourceRectangle.bottom = in_resource_desc.Height;

    in_stream_args.Transform.DestinationRectangle.left = 0;
    in_stream_args.Transform.DestinationRectangle.top = 0;
    in_stream_args.Transform.DestinationRectangle.right = out_resource_desc.Width;
    in_stream_args.Transform.DestinationRectangle.bottom = out_resource_desc.Height;

    in_stream_args.Transform.Orientation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;

    in_stream_args.Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
    in_stream_args.RateInfo.OutputIndex = 0;
    in_stream_args.RateInfo.InputFrameOrField = 0;
    in_stream_args.AlphaBlending.Enable = false;

    D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS out_stream_args = {};
    out_stream_args.OutputStream[0].pTexture2D = dst;
    out_stream_args.OutputStream[0].Subresource = 0;
    out_stream_args.TargetRectangle.left = 0;
    out_stream_args.TargetRectangle.top = 0;
    out_stream_args.TargetRectangle.right = out_resource_desc.Width;
    out_stream_args.TargetRectangle.bottom = out_resource_desc.Height;
    m_video_cmd_list->ProcessFrames(m_video_processor, &out_stream_args, 1, &in_stream_args);

    // set resource state
    D3D12_RESOURCE_BARRIER states_after[2] = {};
    states_after[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[0].Transition.pResource = src;
    states_after[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[0].Transition.StateBefore = D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ;
    states_after[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    states_after[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    states_after[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    states_after[1].Transition.pResource = dst;
    states_after[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    states_after[1].Transition.StateBefore = D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE;
    states_after[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    m_video_cmd_list->ResourceBarrier(2, states_after);

    // close command list
    result = m_video_cmd_list->Close();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12VideoProcessCommandList->Close failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = src_surface->wait_gpu_event_gpu(m_video_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_event_gpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // wait previous copy submission
    result = dst_surface->wait_gpu_event_gpu(m_video_cmd_queue);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_event_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // execute command list
    ID3D12CommandList* command_lists[] = { m_video_cmd_list };
    m_video_cmd_queue->ExecuteCommandLists(1, command_lists);

    // signal gpu fence
    auto fence_value = InterlockedIncrement(&m_vp_fence_value);
    result = m_video_cmd_queue->Signal(m_vp_fence, m_vp_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12CommandQueue->Signal failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = m_vp_fence->SetEventOnCompletion(m_vp_fence_value, m_vp_event);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D12Fence->SetEventOnCompletion failed, result = 0x%08x\n", result);
        return E_FAIL;
    }
    m_vp_event_signalled = true;

    result = dst_surface->signal_gpu_event(m_vp_fence, m_vp_fence_shared_handle, m_vp_fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->signal_gpu_fence failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // update processed frame
    m_processed_frame = Frame::create(std::move(dst_surface), m_output_surface_pool);
    return S_OK;
}

void VideoProcessor::update_output_frame() {
    std::unique_lock output_lk(m_output_lock);
    m_output_frame = m_processed_frame;
    output_lk.unlock();
    m_output_cv.notify_one();
}
