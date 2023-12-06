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

#include <cstdint>
#include <filesystem>

/**
 * @brief      This structure describes encoder parameters
 */
struct EncoderParams {
    // codec id : supported codecs avc, hevc, av1
    enum class Codec {
        unknown,
        avc,
        hevc,
        av1,
    };

    // quality preset
    enum class QualityPreset {
        veryfast, // best speed
        faster,
        fast,
        medium,
        slow,
        slower,
        veryslow, // best quality
    };

    // rate control method
    enum class RateControl {
        cqp,
        vbr,
    };

    // chroma format for encoded bitstream
    enum class OutputChromaFormat {
        chroma420,
        chroma444,
    };

    // codec id
    Codec codec = Codec::unknown;
    // quality preset
    QualityPreset preset = QualityPreset::medium;
    // rate control method
    RateControl rate_control = RateControl::vbr;
    // target bitrate
    uint32_t target_bitrate = 0;
    // key frame interval
    uint32_t key_frame_interval = 0;
    // encoded bitstream frame rate
    uint16_t frame_rate = 0;
    // chroma format for encoded bitstream
    OutputChromaFormat output_chroma_format = OutputChromaFormat::chroma420;
    // display adapter to run encoder
    LUID adapter_luid = {};
};

std::string to_string(const EncoderParams::Codec& codec);
std::string to_string(const EncoderParams::QualityPreset& preset);
std::string to_string(const EncoderParams::RateControl& rc);
std::string to_string(const EncoderParams::OutputChromaFormat& format);

/**
 * @brief      This class describes bitstream packet representing one encoded frame
 */
struct Packet {
    static constexpr uint32_t flag_keyframe = 0x1;

    std::vector<uint8_t> data;
    uint32_t flags = 0;
};

/**
 * @brief      This class describes generic encoder interface
 *             This API works as follows:
 *             - create encoder instance
 *             - create encoding thread and call encode_frame() in a loop
 *             - create receiver thread and call receive_packet() in a loop
 *             Encoder will initialize internal state based on the parameters
 *             from the first frame received. If input frame parameters are changed,
 *             encoder will flush outstanding packets and try to re-initialize
 *             internal state based on new frame parameters.
 */
struct Encoder {

    virtual ~Encoder() = default;

    /**
     * @brief      returns true if input frame format is supported
     *
     * @param[in]  format  DXGI format
     *
     * @return     true, if the specified format is format supported
     *             false, otherwise
     */
    virtual bool is_format_supported(DXGI_FORMAT format) const = 0;

    /**
     * @brief      Start encoder
     *
     * @return     0, on success
     *             HRESULT, om error
     */
    virtual HRESULT start() = 0;

    /**
     * @brief      Stop encoder
     */
    virtual void stop() = 0;

    /**
     * @brief      Encode one frame
     *
     * @param[in]  frame  frame object
     *
     * @return     0, on success
     *             E_INVALIDARG, if frame is nullptr
     *             E_FAIL, on error
     */
    virtual HRESULT encode_frame(Frame* frame) = 0;

    /**
     * @brief      Block thread and wait for a new bitstream packet with timeout.
     *
     * @param[out] packet      bitstream packet
     * @param[in]  timeout_ms  timeout in milliseconds
     *
     * @return     0, on success
     *             DXGI_ERROR_WAIT_TIMEOUT, if timeout interval elapses before new packet arrives
     *             E_FAIL, on error
     */
    virtual HRESULT receive_packet(Packet& packet, UINT timeout_ms) = 0;

    // [fixme] temp desc: flush encoder
    // [fixme] not yet implemented
    // virtual HRESULT end_of_stream() = 0;

    /**
     * @brief      Signal encoder to insert a key frame
     */
    virtual void request_key_frame() = 0;
};
