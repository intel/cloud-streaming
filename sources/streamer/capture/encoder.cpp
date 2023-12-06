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

#include "encoder.h"

std::string to_string(const EncoderParams::Codec& codec) {
    switch (codec) {
    case EncoderParams::Codec::avc:
        return "avc";
    case EncoderParams::Codec::hevc:
        return "hevc";
    case EncoderParams::Codec::av1:
        return "av1";
    }

    return "unknown";
}

std::string to_string(const EncoderParams::Codec& codec, const EncoderParams::Profile& profile) {
    switch (codec) {
    case EncoderParams::Codec::avc:
        switch (profile) {
        case EncoderParams::Profile::avc_baseline:
            return "baseline";
        case EncoderParams::Profile::avc_main:
            return "main";
        case EncoderParams::Profile::avc_high:
            return "high";
        }
    case EncoderParams::Codec::hevc:
        switch (profile) {
        case EncoderParams::Profile::hevc_main:
            return "main";
        case EncoderParams::Profile::hevc_main10:
            return "main10";
        case EncoderParams::Profile::hevc_mainsp:
            return "mainsp";
        case EncoderParams::Profile::hevc_rext:
            return "rext";
        case EncoderParams::Profile::hevc_scc:
            return "scc";
        }
    case EncoderParams::Codec::av1:
        switch (profile) {
        case EncoderParams::Profile::av1_main:
            return "main";
        }
    }

    return "unknown";
}

std::string to_string(const EncoderParams::QualityPreset& preset) {
    switch (preset) {
    case EncoderParams::QualityPreset::veryfast:
        return "veryfast";
    case EncoderParams::QualityPreset::faster:
        return "faster";
    case EncoderParams::QualityPreset::fast:
        return "fast";
    case EncoderParams::QualityPreset::medium:
        return "medium";
    case EncoderParams::QualityPreset::slow:
        return "slow";
    case EncoderParams::QualityPreset::slower:
        return "slower";
    case EncoderParams::QualityPreset::veryslow:
        return "veryslow";
    }

    return "unknown";
}

std::string to_string(const EncoderParams::RateControl& rc) {
    switch (rc) {
    case EncoderParams::RateControl::cqp:
        return "cqp";
    case EncoderParams::RateControl::vbr:
        return "vbr";
    }

    return "unknown";
}

std::string to_string(const EncoderParams::OutputChromaFormat& format) {
    switch (format) {
    case EncoderParams::OutputChromaFormat::chroma420:
        return "4:2:0";
    case EncoderParams::OutputChromaFormat::chroma444:
        return "4:4:4";
    }

    return "unknown";
}
