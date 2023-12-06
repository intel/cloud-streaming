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

#include "av-utils.h"
#include "frame.h"
#include "encoder.h"

#include <dxgi.h>
#include <d3d11_4.h>
#include <atlcomcli.h>

#include <mutex>
#include <fstream>
#include <deque>

struct AVCodecContext; // forward decl
struct AVBufferRef; // forward decl
struct AVQSVDeviceContext; // forward decl
struct AVQSVFramesContext; // forward decl

/**
 * @brief      This class describes FFmpeg QSV video encoder implementation
 */
class AVQSVEncoder : public Encoder {
public:
    virtual ~AVQSVEncoder();

    /**
     * @brief      Create encoder instance
     *
     * @param[in]  enc_params  encoder parameters
     *
     * @return     encoder object, on success
     *             nullptr, on error
     */
    static std::unique_ptr<AVQSVEncoder> create(const EncoderParams& enc_params);

    /**
     * @brief      returns true if input frame format is supported
     *             supported formats:
     *             - DXGI_FORMAT_NV12
     *             - DXGI_FORMAT_B8G8R8A8_UNORM
     *
     * @param[in]  format  DXGI format
     *
     * @return     true, if the specified format is format supported
     *             false, otherwise
     */
    bool is_format_supported(DXGI_FORMAT format) const override;

    /**
     * @brief      Start encoder
     *
     * @return     0, on success
     *             HRESULT, om error
     */
    HRESULT start() override;

    /**
     * @brief      Stop encoder
     */
    void stop() override;

    /**
     * @brief      Encode one frame
     *
     * @param[in]  frame  frame object
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT encode_frame(Frame* frame) override;

    /**
     * @brief      Receive bitstream packet
     *
     * @param[out] packet      bitstream packet
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new packet arrives
     *             E_FAIL, on error
     */
    HRESULT receive_packet(Packet& packet, UINT timeout_ms) override;

    /**
     * @brief      Signal encoder to insert a key frame
     */
    void request_key_frame() override;

private:
    AVQSVEncoder() = default;

    /**
     * @brief      Create and initialize top-level AV codec context.
     *             This function will call the following methods to initialize
     *             hardware-specific state:
     *             init_av_hw_device_context()
     *             init_av_hw_frames_context()
     *             init_av_qsv_device_context()
     *             init_av_qsv_frames_context()
     *
     * @param[in]  frame_width   frame width
     * @param[in]  frame_height  frame height
     * @param[in]  frame_format  frame format
     *
     * @return     0, on success
     *             E_INVALIDARG, if one or more input args are invalid
     *             E_FAIL, on error
     */
    HRESULT init_av_context(uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format);

    /**
     * @brief      Create and initialize AV hardware device context
     *             *hw_device_ctx must be nullptr.
     *
     * @param[out] hw_device_ctx  pointer to AVBufferRef*
     *
     * @return     0, on success
     *             E_INVALIDARG, if hw_device_ctx is nullptr
     *             E_FAIL, on error
     */
    HRESULT init_av_hw_device_context(AVBufferRef** hw_device_ctx);

    /**
     * @brief      Create and initialize AV hardware frame context
     *             *hw_frames_ctx must be nullptr.
     *             frame_width must be greater 0
     *             frame_height must be greater 0
     *
     * @param[out] hw_frames_ctx  pointer to AVBufferRef*
     * @param[in]  hw_device_ctx  AV hardware device context
     * @param[in]  frame_width    frame width
     * @param[in]  frame_height   frame height
     * @param[in]  frame_format   frame format
     *
     * @return     0, on success
     *             E_INVALIDARG, if hw_frames_ctx or hw_device_ctx is nullptr
     *             E_FAIL, on error
     */
    HRESULT init_av_hw_frames_context(AVBufferRef** hw_frames_ctx, AVBufferRef* hw_device_ctx,
        uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format);

    /**
     * @brief      Initialize QSV-specific device context
     *
     * @param      qsv_context  pointer to AVQSVDeviceContext
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT init_av_qsv_device_context(AVQSVDeviceContext* qsv_context);

    /**
     * @brief      Initialize QSV-specific frame context
     *
     * @param      qsv_context  pointer to AVQSVFramesContext
     *
     * @return     0, on success
     *             E_FAIL, on error
     */
    HRESULT init_av_qsv_frames_context(AVQSVFramesContext* qsv_context);

    /**
     * @brief      Copy input surface to encode buffer
     *
     * @param      dst   encode surface
     * @param      src   source surface
     *
     * @return     0, on success
     *             E_INVALIDARG, if src or dst is nullptr
     *             E_FAIL, on error
     */
    HRESULT copy_src_surface_to_encode(ID3D11Texture2D* dst, ID3D11Texture2D* src);

private:
    // encoder parameters
    EncoderParams m_desc = {};

    // adapter to run encode
    DXGI_ADAPTER_DESC m_adapter_desc = {};
    CComPtr<IDXGIAdapter> m_adapter;

    // D3D11 device and context to run encode
    CComPtr<ID3D11Device5> m_device;
    CComPtr<ID3D11DeviceContext4> m_device_context;
    CComPtr<ID3D11Multithread> m_device_context_lock;
    CComPtr<ID3D11Fence> m_fence;
    UINT64 m_fence_value = 0;
    HANDLE m_fence_shared_handle = nullptr;

    // top-level AV codec context
    std::unique_ptr<AVCodecContext, utils::deleter::av_context> m_av_context;

    // input frame desc
    uint32_t m_frame_width = 0;
    uint32_t m_frame_height = 0;
    DXGI_FORMAT m_frame_format = DXGI_FORMAT_UNKNOWN;

    // output bitstream packet queue
    std::mutex m_packet_queue_lock;
    std::condition_variable m_packet_queue_cv;
    std::deque<Packet> m_packet_queue;

    // signal to insert keyframe
    std::atomic<int> m_insert_key_frame = 0;

    // initial encode surface pool size
    static constexpr uint32_t m_init_pool_size = 8;
    // max time to wait on source surface
    static constexpr uint32_t m_src_wait_timeout_ms = 500;
    // maximum size for output packet queue
    static constexpr uint32_t m_packet_queue_max_size = 4;
};
