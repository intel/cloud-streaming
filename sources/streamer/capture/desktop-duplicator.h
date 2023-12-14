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

#include "frame.h"
#include "frame-provider.h"
#include "cursor-provider.h"

#include <atlcomcli.h>
#include <dxgi1_2.h>
#include <d3d11_4.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class DX11SurfacePool; // forward decl

/**
 * @brief      Frame provider class for a single desktop output duplication.
 *             This class uses IDXGIOutputDuplication interface to acquire
 *             desktop texture for futher processing by performing
 *             the following operations in a loop within a thread:
 *             1. acquire desktop texture from Desktop Window Manager (DWM)
 *             2. copy dektop texture to staging texture
 *             3. release desktop texture
 *             User can obtain last captured frame by calling receive_frame()
 *             in a loop. Captured frame has the same size and format as desktop
 *             texture provided by DWM. Upon receiving a frame user must obtain
 *             underlying surface by calling Frame::get_surface() and then
 *             wait on gpu operation completion by calling Surface::wait_gpu_fence_*()
 *             When desktop display mode is changed this class resets internal
 *             state to adjust resolution and/or output format.
 */
class DesktopDuplicator : public FrameProvider, public CursorProvider {
public:
    virtual ~DesktopDuplicator();

    /**
     * @brief      Create instance from display device name (for ex. "\\.\DISPLAY1")
     *             representing a single dektop.
     *             Display device name can be obtained by the following calls
     *             1. IDXGIOutput->GetDesc() : from DXGI_OUTPUT_DESC struct and DeviceName member
     *             2. GetMonitorInfo() : from MONITORINFOEXW struct szDevice member
     *
     * @param[in]  display_device_name  display device name
     *
     * @return     pointer to new instance, on success
     *             nullptr, on failure
     */
    static std::unique_ptr<DesktopDuplicator> create(const std::wstring& display_device_name);

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
     * @brief      Block thread and wait for a new frame with timeout.
     *             Called by user to acquire new frame from display output.
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
     * @brief      Block thread and wait for cursor update.
     *             Called by user to acquire cursor state.
     *
     * @param      cursor_state  cursor state
     * @param[in]  timeout_ms    timeout in milliseconds
     *
     * @return     0 and cursor description, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new frame arrives
     *             E_FAIL, on error
     */
    HRESULT receive_cursor(CursorState& cursor_state, UINT timeout_ms) override;

    /**
     * @brief      Returns DXGI adapter connected to display
     *
     * @return     IDXGIAdapter interface pointer
     */
    IDXGIAdapter* get_display_adapter() const { return m_adapter; }

    /**
     * @brief      Returns DXGI output used for display
     *
     * @return     IDXGIOutput interface pointer
     */
    IDXGIOutput* get_display_output() const { return m_output; }

private:
    DesktopDuplicator() = default;

    bool keep_alive() const { return m_keep_alive.load(); }

    /**
     * @brief      Thread function for desktop duplication
     *
     * @param      context  class interface
     *
     * @return     thread exit code
     *             0, on success
     *             HRESULT, on error
     */
    static HRESULT thread_proc(DesktopDuplicator* context);

    /**
     * @brief      Reset internal output duplication interface
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT reset();

    /**
     * @brief      Acquire latest desktop texture from DWM
     *
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0, on success
     *             DXGI_ERROR_ACCESS_LOST, if DWM mode changes (for ex.
     *             resolution change or DWM transitions to fullscreen application)
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses
     *             before new desktop texture is available
     *             E_FAIL, on error
     */
    HRESULT acquire_surface(UINT timeout_ms);

    /**
     * @brief      Allocate new surface and copy acquired desktop texture
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT copy_surface();
    
    /**
     * @brief      Release desktop texture
     *
     * @return     0, on success
     *             DXGI_ERROR_ACCESS_LOST, if DWM mode changes (for ex.
     *             resolution change or DWM transitions to fullscreen application)
     *             E_FAIL, on error
     */
    HRESULT release_surface();

    bool update_cursor_position(const DXGI_OUTDUPL_FRAME_INFO& frame_info);

    bool update_cursor_shape(const DXGI_OUTDUPL_FRAME_INFO& frame_info);

    HRESULT update_cursor_shape_monochrome(CursorState& desc,
        const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data);

    HRESULT update_cursor_shape_color(CursorState& desc,
        const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data);

    HRESULT update_cursor_shape_masked_color(CursorState& desc,
        const DXGI_OUTDUPL_POINTER_SHAPE_INFO& shape_info, const std::vector<uint8_t>& shape_data);

private:
    // frame event timescale
    using clock_t = FrameTimingInfo::clock_t;
    using duration_t = FrameTimingInfo::duration_t;
    using time_point_t = FrameTimingInfo::time_point_t;

    // selected output display device and desc
    std::wstring m_device_name;
    CComPtr<IDXGIAdapter> m_adapter;
    CComPtr<IDXGIOutput1> m_output;

    DXGI_ADAPTER_DESC m_adapter_desc = {};
    DXGI_OUTPUT_DESC m_output_desc = {};

    // dx11 context used to perform a copy
    CComPtr<ID3D11Device5> m_device;
    CComPtr<ID3D11DeviceContext4> m_device_context;
    CComPtr<ID3D11Multithread> m_device_context_lock;
    CComPtr<ID3D11Fence> m_copy_fence;
    HANDLE m_copy_fence_shared_handle = nullptr;
    UINT64 m_copy_fence_value = 0;

    // output duplication interface
    CComPtr<IDXGIOutputDuplication> m_duplication;
    DXGI_OUTDUPL_DESC m_duplication_desc = {};

    // surface pool for output frame
    std::shared_ptr<DX11SurfacePool> m_surface_pool;

    // desktop texture reference, owned by DWM
    CComPtr<ID3D11Texture2D> m_desktop_texture;

    // internal cursor state
    CursorState m_cursor_state = {};

    std::vector<uint8_t> m_shape_buffer;
    std::vector<uint8_t> m_argb_shape_buffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO m_shape_info = {};

    // syncronization for frame receiver
    std::mutex m_acquire_frame_lock;
    std::condition_variable m_acquire_frame_cv;

    // latest captured frame
    std::shared_ptr<Frame> m_output_frame;

    // syncronization for cursor receiver
    std::mutex m_acquire_cursor_lock;
    std::condition_variable m_acquire_cursor_cv;

    // latest captured cursor
    bool m_cursor_updated = false;
    CursorState m_output_cursor = {};

    // processing thread
    std::thread m_thread;
    std::atomic<int> m_keep_alive = false;
};
