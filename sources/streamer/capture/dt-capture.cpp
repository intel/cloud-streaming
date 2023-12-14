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

#include "dt-capture.h"

#include "dx-utils.h"
#include "desktop-duplicator.h"
#include "video-processor.h"
#include "av-qsv-encoder.h"

DTCapture::~DTCapture() {
    stop();
}

std::unique_ptr<DTCapture> DTCapture::create(DTCaptureParams& capture_params, EncoderParams& encode_params) {
    auto instance = std::unique_ptr<DTCapture>(new DTCapture);

    // set capture config
    instance->m_params = capture_params;

    // create capture object
    instance->m_duplicator = DesktopDuplicator::create(capture_params.display_device_name);
    if (instance->m_duplicator == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": DesktopDuplicator->create failed\n");
        return nullptr;
    }

    HRESULT result = S_OK;

    // configure processing device
    CComPtr<IDXGIAdapter> display_adapter = instance->m_duplicator->get_display_adapter();
    if (display_adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": display adapter is nullptr\n");
        return nullptr;
    }

    DXGI_ADAPTER_DESC display_adapter_desc = {};
    result = display_adapter->GetDesc(&display_adapter_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->GetDesc failed, result = 0x%08x\n", result);
        return nullptr;
    }

    constexpr UINT vendor_intel = 0x8086;

    CComPtr<IDXGIAdapter> encode_adapter;
    if (display_adapter_desc.VendorId != vendor_intel) {
        ga_logger(Severity::WARNING, __FUNCTION__ ": encode is supported only on Intel adapters, selecting first Intel device\n");
        HRESULT result = utils::enum_adapter_by_vendor(&encode_adapter, vendor_intel);
        if (FAILED(result) || encode_adapter == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_adapter_by_vendor failed, result = 0x%08x\n", result);
            return nullptr;
        }
    } else {
        encode_adapter = display_adapter;
    }

    DXGI_ADAPTER_DESC encode_adapter_desc = {};
    result = encode_adapter->GetDesc(&encode_adapter_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->GetDesc failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // create video processor
    VideoProcessor::Desc vp_desc = {};
    vp_desc.adapter_luid = encode_adapter_desc.AdapterLuid;
    vp_desc.frame_rate = encode_params.frame_rate;
    switch (capture_params.output_format) {
    case DTCaptureParams::OutputFormat::nv12:
        vp_desc.output_format = DXGI_FORMAT_NV12;
        break;
    case DTCaptureParams::OutputFormat::rgb:
    default:
        vp_desc.output_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    }

    instance->m_video_processor = VideoProcessor::create(vp_desc);
    if (instance->m_video_processor == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": VideoProcessor->create failed, result = 0x%08x\n", result);
        return nullptr;
    }

    result = instance->m_video_processor->register_frame_provider(instance->m_duplicator);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": VideoProcessor->register_frame_provider failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // create encoder
    encode_params.adapter_luid = vp_desc.adapter_luid;
    instance->m_encoder = AVQSVEncoder::create(encode_params);
    if (instance->m_video_processor == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": VideoProcessor->create failed, result = 0x%08x\n", result);
        return nullptr;
    }

    // create cursor receiver
    CursorReceiverParams cursor_params = {};

    cursor_params.on_error = capture_params.on_error;
    cursor_params.on_cursor_received = capture_params.on_cursor_received;

    instance->m_cursor_receiver = CursorReceiver::create(cursor_params);
    if (instance->m_cursor_receiver == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CursorReceiver->create failed\n");
        return nullptr;
    }
    instance->m_cursor_receiver->register_cursor_provider(instance->m_duplicator);

    return instance;
}

HRESULT DTCapture::start() {
    if (m_duplicator == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": capture provider is nullptr\n");
        return E_FAIL;
    }
    if (m_video_processor == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": video processor is nullptr\n");
        return E_FAIL;
    }
    if (m_encoder == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": encoder is nullptr\n");
        return E_FAIL;
    }

    HRESULT result = S_OK;

    // start capture
    result = m_duplicator->start();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": failed to start capture, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // start video processor
    result = m_video_processor->start();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": failed to start video processor, result = 0x%08x\n", result);
        return E_FAIL;
    }

    ga_logger(Severity::INFO, __FUNCTION__ ": capture started\n");

    // start encode
    result = m_encoder->start();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": failed to start encode, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // start cursor receiver
    result = m_cursor_receiver->start();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": failed to start cursor receiver, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // start threads
    m_keep_alive = 1;
    m_capture_thread = std::move(std::thread(DTCapture::capture_thread_proc, this));
    m_encode_thread = std::move(std::thread(DTCapture::encode_thread_proc, this));

    ga_logger(Severity::INFO, __FUNCTION__ ": encode started\n");

    return S_OK;
}

void DTCapture::stop() {
    if (m_duplicator)
        m_duplicator->stop();
    if (m_video_processor)
        m_video_processor->stop();

    ga_logger(Severity::INFO, __FUNCTION__ ": capture stopped\n");

    if (m_encoder)
        m_encoder->stop();
    if (m_cursor_receiver)
        m_cursor_receiver->stop();

    // stop threads
    m_keep_alive = 0;
    if (m_capture_thread.joinable())
        m_capture_thread.join();
    if (m_encode_thread.joinable())
        m_encode_thread.join();

    ga_logger(Severity::INFO, __FUNCTION__ ": encode stopped\n");
}

void DTCapture::on_key_frame_request() {
    if (m_encoder)
        m_encoder->request_key_frame();
}

HRESULT DTCapture::capture_thread_proc(DTCapture* context) {
    struct LogThreadLifetime {
        LogThreadLifetime() { ga_logger(Severity::INFO, "DTCapture capture thread started\n"); }
        ~LogThreadLifetime() { ga_logger(Severity::INFO, "DTCapture capture thread stoped\n"); }
    } log_thread_lifetime;

    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    if (context->m_video_processor == nullptr) {
        context->on_error(__FUNCTION__ ": frame provider object is nullptr", E_INVALIDARG);
        ga_logger(Severity::ERR, __FUNCTION__ ": frame provider object is nullptr\n");
        return E_INVALIDARG;
    }

    if (context->m_encoder == nullptr) {
        context->on_error(__FUNCTION__ ": encoder object is nullptr", E_INVALIDARG);
        ga_logger(Severity::ERR, __FUNCTION__ ": encoder object is nullptr\n");
        return E_INVALIDARG;
    }

    const auto video_processor = context->m_video_processor.get();
    const auto encoder = context->m_encoder.get();
    const auto capture_timeout_ms = context->m_capture_timeout;

    // capture loop
    while (context->keep_alive()) {
        // capture last processed frame
        std::shared_ptr<Frame> captured_frame;
        HRESULT capture_result = video_processor->receive_frame(captured_frame, capture_timeout_ms);
        if (capture_result == DXGI_ERROR_WAIT_TIMEOUT) {
            continue; // timed out - try again
        } else if (FAILED(capture_result)) {
            context->on_error(__FUNCTION__ ": video_processor->receive_frame failed", capture_result);
            ga_logger(Severity::ERR, __FUNCTION__ ": video_processor->receive_frame failed, result = 0x%08x\n", capture_result);
            continue;
        }

        // update presentation timestamp
        if (captured_frame) {
            using clock_t = FrameTimingInfo::clock_t;
            auto& timing_info = captured_frame->get_timing_info();
            timing_info.presentation_ts = clock_t::now();
        }

        // encode frame
        auto encode_result = encoder->encode_frame(captured_frame.get());
        if (FAILED(encode_result)) {
            context->on_error(__FUNCTION__ ": encoder->encode_frame failed", encode_result);
            ga_logger(Severity::ERR, __FUNCTION__ ": encoder->encode_frame failed, result = 0x%08x\n", encode_result);
            continue;
        }
    }

    return S_OK;
}

HRESULT DTCapture::encode_thread_proc(DTCapture* context) {
    struct LogThreadLifetime {
        LogThreadLifetime() { ga_logger(Severity::INFO, "DTCapture encode thread started\n"); }
        ~LogThreadLifetime() { ga_logger(Severity::INFO, "DTCapture encode thread stoped\n"); }
    } log_thread_lifetime;

    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    if (context->m_encoder == nullptr) {
        context->on_error(__FUNCTION__ ": encoder object is nullptr", E_INVALIDARG);
        ga_logger(Severity::ERR, __FUNCTION__ ": encoder object is nullptr\n");
        return E_INVALIDARG;
    }

    const auto encoder = context->m_encoder.get();
    const auto encode_timeout_ms = context->m_encode_timeout;

    // encode loop
    while (context->keep_alive()) {
        // receive packet from encoder
        Packet packet;
        HRESULT encode_result = encoder->receive_packet(packet, encode_timeout_ms);
        if (encode_result == DXGI_ERROR_WAIT_TIMEOUT) {
            continue; // timed out - try again
        } else if (FAILED(encode_result)) {
            context->on_error(__FUNCTION__ ": encoder->receive_packet failed", encode_result);
            ga_logger(Severity::ERR, __FUNCTION__ ": encoder->receive_packet failed, result = 0x%08x\n", encode_result);
            continue;
        }

        // notify client that packet is ready
        if (context->m_params.on_packet_received) {
            context->m_params.on_packet_received(packet);
        }
    }

    return S_OK;
}
