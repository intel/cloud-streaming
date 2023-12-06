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

#include <string>

extern "C" {
    // forward decl
    struct AVBufferRef;
    struct AVCodecContext;
    struct AVFrame;
    struct AVPacket;
};

namespace utils {
    namespace deleter {

        // AVCodecContext deleter for std::unique_ptr
        struct av_context {
            void operator()(AVCodecContext* p) const;
        };

        // AVBufferRef deleter for std::unique_ptr
        struct av_buffer_ref {
            void operator()(AVBufferRef* p) const;
        };

        // AVFrame deleter for std::unique_ptr
        struct av_frame {
            void operator()(AVFrame* p) const;
        };

        // AVPacket deleter for std::unique_ptr
        struct av_packet {
            void operator()(AVPacket* p) const;
        };

    }; // namespace deleter

    // return error desc for av_result
    std::string av_error_to_string(int av_error);

}; // namespace utils
