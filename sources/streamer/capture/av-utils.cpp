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

#include "av-utils.h"

extern "C" {
#include "libavcodec/avcodec.h"
} // extern "C"

void utils::deleter::av_context::operator()(AVCodecContext* p) const {
    AVCodecContext* tmp = p;
    avcodec_free_context(&tmp);
};

void utils::deleter::av_buffer_ref::operator()(AVBufferRef* p) const {
    AVBufferRef* tmp = p;
    av_buffer_unref(&tmp);
};

void utils::deleter::av_frame::operator()(AVFrame* p) const {
    AVFrame* tmp = p;
    av_frame_free(&tmp);
}

void utils::deleter::av_packet::operator()(AVPacket* p) const {
    AVPacket* tmp = p;
    av_packet_free(&tmp);
}

std::string utils::av_error_to_string(int av_error) {
    if (av_error >= 0)
        return "success";

    std::string error_str(AV_ERROR_MAX_STRING_SIZE, '\0');
    auto result = av_strerror(av_error, error_str.data(), error_str.length());
    if (result < 0)
        return std::string("unknown error");

    return error_str;
}
