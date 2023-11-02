// Copyright (C) 2020-2023 Intel Corporation
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

// clang-format off
#include "video-renderer.h"
#include <processthreadsapi.h>
#include "game-session.h"
#include "ga-option.h"
// clang-format on

extern std::unique_ptr<GameSession> g_remote_connection;

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::system_clock;
using std::chrono::microseconds;
using std::chrono::milliseconds;

static const UINT kBufferCountWithTearing = 3;
static const UINT kBufferCountWithoutTearing = 2;

bool DXRenderer::DXGIIsTearingSupported() {
    IDXGIFactory4* dxgi_factory4 = NULL;
    UINT allow_tearing = 0;

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&dxgi_factory4));
    if (SUCCEEDED(hr) && dxgi_factory4) {
        IDXGIFactory5* dxgi_factory5 = NULL;
        hr = dxgi_factory4->QueryInterface(__uuidof(IDXGIFactory5), (void**)&dxgi_factory5);
        if (SUCCEEDED(hr) && dxgi_factory5) {
            hr = dxgi_factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allow_tearing, sizeof(allow_tearing));
        }
    }
    return SUCCEEDED(hr) && allow_tearing;
}

void DXRenderer::FillSwapChainDesc(DXGI_SWAP_CHAIN_DESC1 &scd) {
    scd.Width = scd.Height = 0; // automatic sizing.
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.Stereo = false;
    scd.SampleDesc.Count = 1; // no multi-sampling
    scd.SampleDesc.Quality = 0;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = kBufferCountWithoutTearing;
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}

void DXRenderer::RenderFrame(std::unique_ptr<owt::base::VideoBuffer> buffer) {

    render_frame_number++;

    HRESULT hr = S_FALSE;
    uint16_t width = 0;
    uint16_t height = 0;

    ID3D11Device *render_device = nullptr;
    ID3D11VideoDevice *render_video_device = nullptr;
    ID3D11Texture2D *texture = nullptr;
    ID3D11VideoContext *render_context = nullptr;
    ID3D11Texture2D *current_back_buffer = nullptr;
    int array_slice = -1;

    auto render_begin = high_resolution_clock::now();
    
    uint64_t client_received_timestamp_ms = duration_cast<milliseconds>(render_begin.time_since_epoch()).count();
    uint64_t decode_duration = 0;

    rapidjson::Document side_data_document;
    bool has_side_data = false;

    FILE* renderLogFile = ga::log::OpenFile("ClientStatsLog", "txt");
    if (renderLogFile == NULL) {
        hr = S_FALSE;
        prev_back_buffer = current_back_buffer;
        prev_texture = texture;
        return;
    }

    // initialize currentFrameStats
    bool has_frame_stats = false;
    if (currentFrameStats == nullptr) {
        currentFrameStats = new FrameStats;
    }
    currentFrameStats->ts = 0;
    currentFrameStats->start_delay = 0;
    currentFrameStats->delay = 0;
    currentFrameStats->size = 0;
    currentFrameStats->p_loss = 0;

    if (!wnd_ || !dxgi_factory_ || !IsWindow(wnd_)) {
        hr = S_FALSE;
    } else {
        owt::base::D3D11VAHandle *handle =
            reinterpret_cast<owt::base::D3D11VAHandle *>(buffer->buffer);

        if (handle) {
            hr = S_OK;
            width = buffer->resolution.width;
            height = buffer->resolution.height;

            if (width == 0 || height == 0) {
                hr = S_FALSE;
            } else {
                render_device = handle->d3d11_device;
                render_video_device = handle->d3d11_video_device;
                texture = handle->texture;
                render_context = handle->context;
                array_slice = handle->array_index;

                // populate currentFrameStats
                currentFrameStats->delay = (int) (handle->last_duration - handle->start_duration);
                currentFrameStats->size = handle->frame_size;
                currentFrameStats->p_loss = handle->packet_loss; // percent
                // This stat must use system_clock since its used for calculating a statistic across systems.
                // high_resolution_clock does not account for time zone differences and so reports erroneous values.
                // Other stats which are purely local should still use high_resolution_clock.
                currentFrameStats->latencymsg_ = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                has_frame_stats = true;

                //send currentFrameStats
                g_remote_connection->SendFrameStats(currentFrameStats);

                D3D11_TEXTURE2D_DESC texture_desc;
                if (texture) {
                    texture->GetDesc(&texture_desc);
                }
                if (render_device == nullptr || render_video_device == nullptr ||
                    texture == nullptr || render_context == nullptr) {
                    hr = S_FALSE;
                } else {
                    if (render_device != d3d11_device_ ||
                        render_video_device != d3d11_video_device_ ||
                        render_context != d3d11_video_context_) {
                        d3d11_device_ = render_device;
                        d3d11_video_device_ = render_video_device;
                        d3d11_video_context_ = render_context;
                        need_swapchain_recreate = true;
                    }
                }

                // handle E2ELatency message
                if (FLAGS_verbose) {
                    ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Side Data Size: %lu\n", render_frame_number, handle->side_data_size);
                }
                if (handle->side_data_size > 0) {
                    std::string message;
                    // get message pointer
                    message.append((char*)handle->side_data, handle->side_data_size);

                    decode_duration = (handle->decode_end - handle->decode_start); // decode times are already in ms

                    if (ga::json::ParseMessage(side_data_document, message)) {
                        // Check if side data is corrupted. Ignore if this is the case.
                        has_side_data = true;
                    }
                }
                // end handling E2ELatency message
            }
        }
    }

    if (hr == S_OK && need_swapchain_recreate) {

        if (swap_chain_for_hwnd_) {
            swap_chain_for_hwnd_->Release();
        }
        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {0};
        FillSwapChainDesc(swap_chain_desc);

        hr = dxgi_factory_->CreateSwapChainForHwnd(d3d11_device_, wnd_,
                                                   &swap_chain_desc, nullptr,
                                                   nullptr, &swap_chain_for_hwnd_);

        if (SUCCEEDED(hr)) {
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;
            memset(&content_desc, 0, sizeof(content_desc));

            // Non-scaling
            content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
            content_desc.InputFrameRate.Numerator = 1000;
            content_desc.InputFrameRate.Denominator = 1;
            content_desc.InputWidth    = width;
            content_desc.InputHeight = height;
            content_desc.OutputWidth = width_;
            content_desc.OutputHeight = height_;
            content_desc.OutputFrameRate.Numerator = 1000;
            content_desc.OutputFrameRate.Denominator = 1;
            content_desc.Usage = D3D11_VIDEO_USAGE_OPTIMAL_SPEED;
            if (FLAGS_verbose) {
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server Resolution: %lu x %lu\n", render_frame_number, width, height);
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Resolution: %lu x %lu\n", render_frame_number, width_, height_);
                double wscale = width_ / (double) width;
                double hscale = height_ / (double) height;
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server to Client Scaling: %.3f x %.3f = %.3f\n", render_frame_number, wscale, hscale, wscale * hscale);
            }

            HRESULT hr = d3d11_video_device_->CreateVideoProcessorEnumerator(
                             &content_desc, &video_processors_enum_);

            if (SUCCEEDED(hr)) {
                if (video_processor_) {
                    video_processor_->Release();
                }
                hr = d3d11_video_device_->CreateVideoProcessor(video_processors_enum_,
                                                               0, &video_processor_);

                if (SUCCEEDED(hr)) {
                    RECT render_rect = {x_offset_, y_offset_, x_offset_ + width_,
                                        y_offset_ + height_};
                    d3d11_video_context_->VideoProcessorSetOutputTargetRect(
                        video_processor_, TRUE, &render_rect);
                } else {
                }
            } else {
            }
        } else {
        }

        need_swapchain_recreate = false;
    }

    if (SUCCEEDED(hr)) {
        hr = swap_chain_for_hwnd_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                             (void **)&current_back_buffer);

        if (SUCCEEDED(hr)) {
            if (prev_back_buffer != current_back_buffer) {

                // Create output view and input view
                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc;
                memset(&output_view_desc, 0, sizeof(output_view_desc));

                output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
                output_view_desc.Texture2D.MipSlice = 0;

                hr = d3d11_video_device_->CreateVideoProcessorOutputView(
                    current_back_buffer, video_processors_enum_, &output_view_desc,
                    &output_view_);

                if (FAILED(hr)) {
                }
            }

            if (SUCCEEDED(hr)) {
                prev_array_slice_ = array_slice;
                D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc;
                memset(&input_view_desc, 0, sizeof(input_view_desc));
                input_view_desc.FourCC = 0;
                input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
                input_view_desc.Texture2D.MipSlice = 0;
                input_view_desc.Texture2D.ArraySlice = array_slice;

                hr = d3d11_video_device_->CreateVideoProcessorInputView(
                    texture, video_processors_enum_, &input_view_desc, &input_view_);

                if (SUCCEEDED(hr)) {
                    // Blit NV12 surface to RGB back buffer here.
                    RECT rect = {0, 0, width_, height_};
                    // Make sure we are using the cropped rectangle within input texture.
                    RECT src_rect = { 0, 0, width, height };
                    memset(&stream_, 0, sizeof(stream_));
                    stream_.Enable = true;
                    stream_.OutputIndex = 0;
                    stream_.InputFrameOrField = 0;
                    stream_.PastFrames = 0;
                    stream_.ppPastSurfaces = nullptr;
                    stream_.ppFutureSurfaces = nullptr;
                    stream_.pInputSurface = input_view_;
                    stream_.ppPastSurfacesRight = nullptr;
                    stream_.ppFutureSurfacesRight = nullptr;
                    stream_.pInputSurfaceRight = nullptr;

                    d3d11_video_context_->VideoProcessorSetStreamSourceRect(video_processor_, 0, true, &src_rect);
                    d3d11_video_context_->VideoProcessorSetStreamDestRect(
                        video_processor_, 0, true, &rect);
                    d3d11_video_context_->VideoProcessorSetStreamFrameFormat(
                        video_processor_, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

                    D3D11_VIDEO_PROCESSOR_COLOR_SPACE stream_color_space;
                    memset(&stream_color_space, 0, sizeof(stream_color_space));
                    stream_color_space.Usage = 0;        // Used for playback
                    stream_color_space.RGB_Range = 1;    // RGB limited range
                    stream_color_space.YCbCr_Matrix = 1; // BT.709
                    stream_color_space.YCbCr_xvYCC = 0;  // Conventional YCbCr
                    stream_color_space.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
                    d3d11_video_context_->VideoProcessorSetStreamColorSpace(video_processor_, 0, &stream_color_space);

                    D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_color_space;
                    memset(&output_color_space, 0, sizeof(output_color_space));
                    output_color_space.Usage = 0;        // Used for playback
                    output_color_space.RGB_Range = 0;    // RGB full range
                    output_color_space.YCbCr_Matrix = 1; // BT.709
                    output_color_space.YCbCr_xvYCC = 0;  // Conventional YCbCr
                    output_color_space.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
                    d3d11_video_context_->VideoProcessorSetOutputColorSpace(video_processor_, &output_color_space);
                } else {
                }
            }
        } else {
        }
    }

    if (SUCCEEDED(hr)) {
        hr = d3d11_video_context_->VideoProcessorBlt(video_processor_, output_view_,
                                                     0, 1, &stream_);
        if (FAILED(hr)) {
        }
    }

    prev_back_buffer = current_back_buffer;
    prev_texture = texture;

    if (SUCCEEDED(hr)) {
        DXGI_PRESENT_PARAMETERS parameters = {0};
        hr = swap_chain_for_hwnd_->Present1(0, 0, &parameters);
    }

    //E2ELatency
    auto render_end = high_resolution_clock::now();
    auto client_render_time = duration_cast<milliseconds>(render_end - render_begin).count();

    if (has_frame_stats && currentFrameStats) {
        if (_streamingStatistics && render_frame_number > 1) {
            _streamingStatistics->CalcStatistics((double)currentFrameStats->size,
                &(_streamingStatistics->framesizerealtime), &(_streamingStatistics->framesizeavgtime),
                &(_streamingStatistics->framesizemintime), &(_streamingStatistics->framesizemaxtime),
                &(_streamingStatistics->framesizeindex), &(_streamingStatistics->framesizetimesum),
                _streamingStatistics->framesizetimelist);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Size: Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->framesizerealtime,
                _streamingStatistics->framesizeavgtime,
                _streamingStatistics->framesizemintime,
                _streamingStatistics->framesizemaxtime);
            
            _streamingStatistics->CalcStatistics((double)currentFrameStats->delay,
                &(_streamingStatistics->framedelayrealtime), &(_streamingStatistics->framedelayavgtime),
                &(_streamingStatistics->framedelaymintime), &(_streamingStatistics->framedelaymaxtime),
                &(_streamingStatistics->framedelayindex), &(_streamingStatistics->framedelaytimesum),
                _streamingStatistics->framedelaytimelist);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Delay: Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->framedelayrealtime,
                _streamingStatistics->framedelayavgtime,
                _streamingStatistics->framedelaymintime,
                _streamingStatistics->framedelaymaxtime);
            
            _streamingStatistics->CalcStatistics((double)currentFrameStats->p_loss,
                &(_streamingStatistics->packetlossrealtime), &(_streamingStatistics->packetlossavgtime),
                &(_streamingStatistics->packetlossmintime), &(_streamingStatistics->packetlossmaxtime),
                &(_streamingStatistics->packetlossindex), &(_streamingStatistics->packetlosstimesum),
                _streamingStatistics->packetlosstimelist);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Loss (%%): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->packetlossrealtime,
                _streamingStatistics->packetlossavgtime,
                _streamingStatistics->packetlossmintime,
                _streamingStatistics->packetlossmaxtime);
        } else {
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Size: %ld\n", render_frame_number, currentFrameStats->size);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Delay: %d\n", render_frame_number, currentFrameStats->delay);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Frame Loss (%%): %ld\n", render_frame_number, currentFrameStats->p_loss);
        }
    }

    if (has_side_data) {
        uint64_t client_send_timestamp_ms = ga::json::FromUint64(side_data_document, "clientSendLatencyTime");
        uint64_t server_received_timestamp_ms = ga::json::FromUint64(side_data_document, "serverReceivedLatencyTime");

        uint64_t e2e_latency = (uint64_t)(server_received_timestamp_ms - client_send_timestamp_ms);
        int client_decode_duration = (int)decode_duration;

        if (client_send_timestamp_ms == 0 || server_received_timestamp_ms == 0) {
            e2e_latency = 0;
        } else {
            if (FLAGS_verbose) {
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Input Timestamp (ms): %llu\n", render_frame_number, client_send_timestamp_ms);
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Received Timestamp (ms): %llu\n", render_frame_number, client_received_timestamp_ms);
            }

            if (_streamingStatistics && render_frame_number > 1) {
                _streamingStatistics->CalcStatistics((double)e2e_latency,
                &(_streamingStatistics->e2erealtime), &(_streamingStatistics->e2eavgtime),
                &(_streamingStatistics->e2emintime), &(_streamingStatistics->e2emaxtime),
                &(_streamingStatistics->e2eindex), &(_streamingStatistics->e2etimesum),
                _streamingStatistics->e2etimelist);

                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, E2E Latency (ms): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                    render_frame_number,
                    _streamingStatistics->e2erealtime,
                    _streamingStatistics->e2eavgtime,
                    _streamingStatistics->e2emintime,
                    _streamingStatistics->e2emaxtime);
            } else {
                ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, E2E Latency (ms): %llu\n", render_frame_number, e2e_latency);
            }
        }

        if (_streamingStatistics) {
            _streamingStatistics->framewidth = width;
            _streamingStatistics->frameheight = height;
        }
        
        if (_streamingStatistics && render_frame_number > 1) {
            _streamingStatistics->CalcStatistics((double)client_decode_duration,
                &(_streamingStatistics->decrealtime), &(_streamingStatistics->decavgtime),
                &(_streamingStatistics->decmintime), &(_streamingStatistics->decmaxtime),
                &(_streamingStatistics->decindex), &(_streamingStatistics->dectimesum),
                _streamingStatistics->dectimelist);
            _streamingStatistics->CalcStatistics((double)client_render_time,
                &(_streamingStatistics->crenrealtime), &(_streamingStatistics->crenavgtime),
                &(_streamingStatistics->crenmintime), &(_streamingStatistics->crenmaxtime),
                &(_streamingStatistics->crenindex), &(_streamingStatistics->crentimesum),
                _streamingStatistics->crentimelist);
            uint64_t server_encode_duration = ga::json::FromUint64(side_data_document, "serverEncodeFrameTime");
            _streamingStatistics->CalcStatistics((double)server_encode_duration,
                &(_streamingStatistics->encrealtime), &(_streamingStatistics->encavgtime),
                &(_streamingStatistics->encmintime), &(_streamingStatistics->encmaxtime),
                &(_streamingStatistics->encindex), &(_streamingStatistics->enctimesum),
                _streamingStatistics->enctimelist);
            uint64_t server_render_time = ga::json::FromUint64(side_data_document, "serverRenderClientInputTime");
            _streamingStatistics->CalcStatistics((double)server_render_time,
                &(_streamingStatistics->srenrealtime), &(_streamingStatistics->srenavgtime),
                &(_streamingStatistics->srenmintime), &(_streamingStatistics->srenmaxtime),
                &(_streamingStatistics->srenindex), &(_streamingStatistics->srentimesum),
                _streamingStatistics->srentimelist);

            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Decode Duration (ms): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->decrealtime,
                _streamingStatistics->decavgtime,
                _streamingStatistics->decmintime,
                _streamingStatistics->decmaxtime);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Render Duration (ms): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->crenrealtime,
                _streamingStatistics->crenavgtime,
                _streamingStatistics->crenmintime,
                _streamingStatistics->crenmaxtime);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server Encode Duration (ms): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->encrealtime,
                _streamingStatistics->encavgtime,
                _streamingStatistics->encmintime,
                _streamingStatistics->encmaxtime);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server Render Client Input Duration (ms): Real=%.3f, Avg=%.3f, Min=%.3f, Max=%.3f\n",
                render_frame_number,
                _streamingStatistics->srenrealtime,
                _streamingStatistics->srenavgtime,
                _streamingStatistics->srenmintime,
                _streamingStatistics->srenmaxtime);
        } else {
            // We aren't tracking stats or verbosity is not enabled so just display current values.
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Decode Duration (ms): %d\n", render_frame_number, client_decode_duration);
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Render Duration (ms): %d\n", render_frame_number, client_render_time);
            uint64_t server_encode_duration = ga::json::FromUint64(side_data_document, "serverEncodeFrameTime");
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server Encode Duration (ms): %d\n", render_frame_number, server_encode_duration);
            uint64_t server_render_time = ga::json::FromUint64(side_data_document, "serverRenderClientInputTime");
            ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Server Render Client Input Duration (ms): %d\n", render_frame_number, server_render_time);
        }
    } else if (FLAGS_verbose) {
        ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, No message from Latency Client Instance: %d\n", render_frame_number);
    }
    
    double frame_to_frame = (double)(duration_cast<milliseconds>(render_end - render_prev).count()) / 1000.0;
    ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Frame Time (s): %.6f\n", render_frame_number, frame_to_frame);

    if (_streamingStatistics && render_frame_number > 1) {
        _streamingStatistics->CalcFPS(frame_to_frame);
        ga::log::WriteToMsg(render_stats_log_msg, "Frame Number: %u, Client Capture FPS: %lu\n", render_frame_number, _streamingStatistics->captureFps);
        _streamingStatistics->updated = true;
    }

    render_prev = render_end;

    ga::log::FlushMsgToFile(renderLogFile, render_stats_log_msg);
    ga::log::CloseFile(renderLogFile);

    return;
}

void DXRenderer::Cleanup() {
    if (swap_chain_for_hwnd_) {
        swap_chain_for_hwnd_->Release();
    }
    if (currentFrameStats) {
        delete currentFrameStats;
    }
}

