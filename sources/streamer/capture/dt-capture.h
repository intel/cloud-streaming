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
#include "encoder.h"
#include "cursor-receiver.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class DesktopDuplicator; // forward decl
class VideoProcessor; // forward decl

struct DTCaptureParams {
    enum class OutputFormat {
        rgb,
        nv12,
    };
    
    // display device name
    std::wstring display_device_name;
    // capture output surface format
    OutputFormat output_format;
    // notification callback that next packet is ready
    std::function<void(const Packet& packet)> on_packet_received;
    // notification callback that next cursor is ready
    std::function<void(const CursorState&)> on_cursor_received;
    // notification callback that some error has occurred
    std::function<void(const std::string&, HRESULT)> on_error;
};

/**
 * @brief      This class describes a desktop capture interface
 */
class DTCapture {
public:
    virtual ~DTCapture();

    /**
     * @brief      Create new capture instance
     *             This call may adjust capture or encode params
     *
     * @param      capture_params  capture parameters
     * @param      encode_params   encode parameters
     *
     * @return     capture object, on success
     *             nullptr, on error
     */
    static std::unique_ptr<DTCapture> create(DTCaptureParams& capture_params, EncoderParams& encode_params);

    /**
     * @brief      Start capture
     *
     * @return     0, on success
     *             HRESULT, on error
     */
    HRESULT start();

    /**
     * @brief      Stop capture
     */
    void stop();

    /**
     * @brief      Callback - request encoder to insert key frame
     */
    void on_key_frame_request();

private:
    DTCapture() = default;

    bool keep_alive() const { return m_keep_alive.load(); }

    /**
     * @brief      Capture and encode submission thread
     *
     * @param      context  DTCapture instance
     *
     * @return     thread exit code
     *             0, on success
     *             HRESULT, on error
     */
    static HRESULT capture_thread_proc(DTCapture* context);

    /**
     * @brief      Encode receiver thread
     *
     * @param      context  DTCapture instance
     *
     * @return     thread exit code
     *             0, on success
     *             HRESULT, on error
     */
    static HRESULT encode_thread_proc(DTCapture* context);

    /**
     * @brief      Send packet over network
     *
     * @param[in]  packet  packet struct
     *
     * @return     0, on success
     *             HRESULT, on error
     */
    HRESULT send_packet(const Packet& packet);

    /**
     * @brief      Safe wrapper to call on_error_cb
     *
     * @param[in]  msg  error message
     * @param[in]  res  error status
     */
    void on_error(std::string msg, HRESULT res) const {
        if (m_params.on_error)
            m_params.on_error(std::move(msg), res);
    }

private:
    // desktop capture parameters
    DTCaptureParams m_params = {};

    // pipeline objects
    std::shared_ptr<DesktopDuplicator> m_duplicator;
    std::unique_ptr<VideoProcessor> m_video_processor;
    std::unique_ptr<Encoder> m_encoder;
    std::unique_ptr<CursorReceiver> m_cursor_receiver;

    // timeout config
    static constexpr UINT m_capture_timeout = 500;
    static constexpr UINT m_encode_timeout = 500;

    // processing thread
    std::thread m_capture_thread;
    std::thread m_encode_thread;
    std::atomic<int> m_keep_alive = false;
};
