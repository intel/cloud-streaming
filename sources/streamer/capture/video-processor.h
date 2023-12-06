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

#include "frame-provider.h"
#include "frame.h"

#include <atlcomcli.h>
#include <d3d12.h>
#include <d3d12video.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

class DX12Surface; // forward decl
class DX12SurfacePool; // forward decl

/**
 * @brief      This class describes a video processor that can be used to:
 *             - convert input frame format
 *             - crop frames
 *             - copy surface across adapters
 *             - provide frames at specified frame rate
 *             User can obtain last processed frame by calling receive_frame() in a loop.
 *             This class creates 2 threads:
 *             - receiver thread : calls FrameProvider->receive_frame() in a loop
 *               and updates input frame for video processor.
 *             - processing thread : processes input frames in a loop and
 *               outputs frames at specified frame rate
 *             Video processor will initialize internal state based on the parameters of the first frame received.
 *             If input frame parameters are changed, video processor will re-initialize internal state on the fly.
 */
class VideoProcessor : public FrameProvider {
public:
    struct Desc {
        // video processor device LUID
        LUID adapter_luid = {};
        // video processor output frame rate
        uint32_t frame_rate = 0;
        // frame output format
        DXGI_FORMAT output_format = DXGI_FORMAT_UNKNOWN;
    };

    ~VideoProcessor();

    /**
     * @brief      Create new video processor instance
     *
     * @param[in]  desc  video processor description
     *
     * @return     video processor object, on success
     *             nullptr, on error
     */
    static std::unique_ptr<VideoProcessor> create(const VideoProcessor::Desc& desc);

    /**
     * @brief      Start frame capture
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT start() override;

    /**
     * @brief      Stop frame capture
     */
    void stop() override;

    /**
     * @brief      Validate video processor parameters
     *
     * @param[in]  desc  video processor description
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    static HRESULT validate_video_processor_desc(const VideoProcessor::Desc& desc);

    /**
     * @brief      Block thread and wait for a new frame in output buffer with timeout.
     *             Called by user to acquire new frame from video processor.
     *
     * @param      frame       frame object
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0 and frame object, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new frame arrives
     *             E_FAIL, on error
     */
    HRESULT receive_frame(std::shared_ptr<Frame>& frame, UINT timeout_ms) override;

    /**
     * @brief      Register frame provider
     *
     * @param[in]  frame_provider  frame provider interface
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT register_frame_provider(std::shared_ptr<FrameProvider> frame_provider);

private:
    using duration_t = std::chrono::steady_clock::duration;
    using time_point_t = std::chrono::steady_clock::time_point;

    VideoProcessor() = default;

    bool keep_alive() const { return m_keep_alive.load(); }

    /**
     * @brief      Update input frame for video processor
     *
     * @param[in]  frame  The frame
     */
    void update_input_frame(std::shared_ptr<Frame> frame);

    /**
     * @brief      Thread function for frame processing
     *
     * @param      context  video processor interface
     *
     * @return     thread exit code
     *             0, on success
     *             HRESULT, on error
     */
    static HRESULT processing_thread_proc(VideoProcessor* context);

    /**
     * @brief      Reset internal state for cross-adapter copy
     *
     * @param[in]  src_device_luid  source device luid
     * @param[in]  frame_width      frame width
     * @param[in]  frame_height     frame height
     * @param[in]  frame_format     frame format
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT reset_copy_processor(const LUID& src_device_luid, uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format);

    /**
     * @brief      Reset internal state for video processing operation
     *
     * @param[in]  src_frame_width   source frame width
     * @param[in]  src_frame_height  source frame height
     * @param[in]  src_frame_format  source frame format
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT reset_video_processor(uint32_t src_frame_width, uint32_t src_frame_height, DXGI_FORMAT src_frame_format);

    /**
     * @brief      Copy input surface to cross-adapter shared local memory pool
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT copy_src_to_staging();

    /**
     * @brief      Copy staging surface from cross-adapter shared local memory pool to video processor device
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT copy_staging_to_dst();

    /**
     * @brief      Process frame :
     *             - copy surface to target adapter if needed
     *             - convert frame to desired format
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT process_frame();

    /**
     * @brief      Move processed frame to output buffer
     */
    void update_output_frame();

    /**
     * @return     Return frame provider object
     */
    std::shared_ptr<FrameProvider> get_frame_provider() { return m_frame_provider; }

private:
    // video processor options
    UINT m_frame_rate = 0;
    DXGI_FORMAT m_output_format = DXGI_FORMAT_UNKNOWN;

    // frame interval duration for frame rate control
    duration_t m_frame_interval = std::chrono::nanoseconds(0);

    // frame provider interface
    std::shared_ptr<FrameProvider> m_frame_provider;

    // thread objects
    std::thread m_processing_thread;
    std::atomic<int> m_keep_alive = false;

    // timeout config
    static constexpr UINT m_gpu_fence_timeout = 500;

    // source and destination device LUIDs
    LUID m_src_device_luid = {};
    LUID m_dst_device_luid = {};

    // D3D12 video processor device and context
    CComPtr<ID3D12Device> m_device;
    CComPtr<ID3D12VideoDevice> m_video_device;
    CComPtr<ID3D12CommandAllocator> m_video_cmd_alloc;
    CComPtr<ID3D12CommandQueue> m_video_cmd_queue;
    CComPtr<ID3D12VideoProcessCommandList> m_video_cmd_list;
    CComPtr<ID3D12Fence> m_vp_fence;
    HANDLE m_vp_fence_shared_handle = nullptr;
    UINT64 m_vp_fence_value;
    HANDLE m_vp_event = nullptr;
    bool m_vp_event_signalled = false;

    D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC m_vp_in_stream_desc = {};
    D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC m_vp_out_stream_desc = {};
    CComPtr<ID3D12VideoProcessor> m_video_processor;

    // output surface pool
    std::shared_ptr<SurfacePool> m_output_surface_pool;

    // cross-adapter copy
    bool m_cross_adapter_copy_needed = false;

    // src to staging context
    CComPtr<ID3D12Device> m_src_copy_device;
    CComPtr<ID3D12CommandAllocator> m_src_copy_cmd_alloc;
    CComPtr<ID3D12CommandQueue> m_src_copy_cmd_queue;
    CComPtr<ID3D12GraphicsCommandList> m_src_copy_cmd_list;
    CComPtr<ID3D12Fence> m_src_copy_fence;
    HANDLE m_src_copy_fence_shared_handle = nullptr;
    UINT64 m_src_copy_fence_value;
    HANDLE m_src_copy_event = nullptr;
    bool m_src_copy_event_signalled = false;

    // staging to dst context
    CComPtr<ID3D12CommandAllocator> m_dst_copy_cmd_alloc;
    CComPtr<ID3D12CommandQueue> m_dst_copy_cmd_queue;
    CComPtr<ID3D12GraphicsCommandList> m_dst_copy_cmd_list;
    CComPtr<ID3D12Fence> m_dst_copy_fence;
    HANDLE m_dst_copy_fence_shared_handle = nullptr;
    UINT64 m_dst_copy_fence_value;
    HANDLE m_dst_copy_event = nullptr;
    bool m_dst_copy_event_signalled = false;

    // copy surfaces and refs
    std::shared_ptr<Frame> m_copy_src_frame;
    std::unique_ptr<DX12Surface> m_copy_staging_surface;
    std::unique_ptr<DX12Surface> m_copy_dst_surface;
    CComPtr<ID3D12Resource> m_src_device_copy_src;
    CComPtr<ID3D12Resource> m_src_device_copy_dst;
    CComPtr<ID3D12Resource> m_dst_device_copy_src;
    CComPtr<ID3D12Resource> m_dst_device_copy_dst;

    // video processor input frame
    std::mutex m_input_lock;
    std::shared_ptr<Frame> m_input_frame;
    CComPtr<ID3D12Resource> m_input_src;

    // video processor output frame
    std::shared_ptr<Frame> m_processed_frame;

    // output buffer
    std::mutex m_output_lock;
    std::condition_variable m_output_cv;
    std::shared_ptr<Frame> m_output_frame;
};

