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

#include "ga-common.h"
#include "ga-conf.h"

#include <charconv>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "dt-capture.h"

static const char* default_bitrate = "3000000";
static const char* default_bitstream_frames_count = "-1";
static const char* default_codec = "h264";
static const char* default_profile = "unknown";
static const char* default_display = ":0";
static const char* default_fps = "60";
static const char* default_gop = default_fps;
static const char* default_loglevel = "none";
static const char* default_rc = "vbr";

void usage(const char* app) {
    printf("usage: %s [options] <output_file>\n", app);
    printf("\n");
    printf("<output_file> is raw bitstream. For avc, hevc or av1 codecs bitstream format\n");
    printf("is defined by Annex B of respective codec specification.\n");
    printf("\n");
    printf("Global options:\n");
    printf("  -h, --help              Print this help\n");
    printf("  --loglevel <level>      Loglevel to use (default: %s)\n", default_loglevel);
    printf("              error         Only errors will be printed\n");
    printf("              warning       Errors and warnings will be printed\n");
    printf("              info          Errors, warnings and info messages will be printed\n");
    printf("              debug         Everything will be printed, including lowlevel debug messages\n");
    printf("              none          Don't write logs to file (errors will still be printed to stdout)'\n");
    printf("\n");
    printf("Capture options:\n");
    printf("  --display <display>     Display output to grab (default: %s)\n", default_display);
    printf("  -n <int>                Number of encoded frames to dump (-1 means infinite). (default: %s)\n", default_bitstream_frames_count);
    printf("\n");
    printf("Video encoding options:\n");
    printf("  --codec <codec>         Video codec (default: %s)\n", default_codec);
    printf("          av1\n");
    printf("          h264 or avc\n");
    printf("          h265 or hevc\n");
    printf("  --profile <profile>      Codec profile (default: %s)\n", default_profile);
    printf("        For av1:\n");
    printf("            main\n");
    printf("        For avc:\n");
    printf("            baseline\n");
    printf("            main\n");
    printf("            high\n");
    printf("        For hevc:\n");
    printf("            main\n");
    printf("            main10\n");
    printf("            mainsp\n");
    printf("            rext\n");
    printf("            scc\n");
    printf("  --bitrate <int>         Video bitrate (default: %s)\n", default_bitrate);
    printf("  --fps <int>             Video fps (default: %s)\n", default_fps);
    printf("  --gop <int>             Video GOP (default: %s)\n", default_gop);
    printf("  --rc cqp|vbr            Video rate control mode (default: %s)\n", default_rc);
}

bool arg_to_int(const std::string& arg, int& value) {
    auto result = std::from_chars(arg.c_str(), arg.c_str() + arg.size(), value);
    if (result.ec != std::errc{})
        return false; // error

    return true; // ok
}

static HRESULT convert_utf8_to_utf16(const std::string& src, std::wstring& dst) {
    if (src.empty()) {
        dst = L"";
        return S_OK;
    }

    int dst_len = MultiByteToWideChar(CP_UTF8, 0, src.data(), src.size(), nullptr, 0);
    if (dst_len <= 0)
        return HRESULT_FROM_WIN32(GetLastError());

    std::wstring utf16_str(dst_len, L'\0');
    dst_len = MultiByteToWideChar(CP_UTF8, 0, src.data(), src.size(), utf16_str.data(), dst_len);
    if (dst_len <= 0)
        return HRESULT_FROM_WIN32(GetLastError());

    dst = std::move(utf16_str);
    return S_OK;
};

static std::string to_string(DTCaptureParams::OutputFormat format) {
    switch (format) {
    case DTCaptureParams::OutputFormat::rgb:
        return "rgb";
    case DTCaptureParams::OutputFormat::nv12:
        return "nv12";
    }
    return "unknown";
}

EncoderParams::Profile to_profile(const EncoderParams::Codec& codec, const std::string& profile) {
    switch (codec) {
    case EncoderParams::Codec::avc:
        if (profile == "baseline")
            return EncoderParams::Profile::avc_baseline;
        else if (profile == "main")
            return EncoderParams::Profile::avc_main;
        else if (profile == "high")
            return EncoderParams::Profile::avc_high;
        break;
    case EncoderParams::Codec::hevc:
        if (profile == "main")
            return EncoderParams::Profile::hevc_main;
        else if (profile == "main10")
            return EncoderParams::Profile::hevc_main10;
        else if (profile == "mainsp")
            return EncoderParams::Profile::hevc_mainsp;
        else if (profile == "rext")
            return EncoderParams::Profile::hevc_rext;
        else if (profile == "scc")
            return EncoderParams::Profile::hevc_scc;
        break;
    case EncoderParams::Codec::av1:
        if (profile == "main")
            return EncoderParams::Profile::av1_main;
        break;
    }
    return EncoderParams::Profile::unknown;
}

static void log_capture_params(const DTCaptureParams& params) {
    const std::string prefix = "desktop-capture:";

    fprintf(stdout, "%s --- capture config:\n", prefix.c_str());
    fprintf(stdout, "%s %s = %S\n", prefix.c_str(), "display_device_name", params.display_device_name.c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "output_format", to_string(params.output_format).c_str());

    ga_logger(Severity::INFO, "%s --- capture config:\n", prefix.c_str());
    ga_logger(Severity::INFO, "%s %s = %S\n", prefix.c_str(), "display_device_name", params.display_device_name.c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "output_format", to_string(params.output_format).c_str());
}

static void log_encode_params(const EncoderParams& params) {
    const std::string prefix = "desktop-capture:";

    fprintf(stdout, "%s --- encode config:\n", prefix.c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "codec", to_string(params.codec).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "profile", to_string(params.codec, params.profile).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "preset", to_string(params.preset).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "rate_control", to_string(params.rate_control).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "target_bitrate", std::to_string(params.target_bitrate).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "key_frame_interval", std::to_string(params.key_frame_interval).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "frame_rate", std::to_string(params.frame_rate).c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "output_chroma_format", to_string(params.output_chroma_format).c_str());
    fprintf(stdout, "%s %s = 0x%x:0x%x\n", prefix.c_str(), "adapter_luid", params.adapter_luid.HighPart, params.adapter_luid.LowPart);

    ga_logger(Severity::INFO, "%s --- encode config:\n", prefix.c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "codec", to_string(params.codec).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "profile", to_string(params.codec, params.profile).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "preset", to_string(params.preset).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "rate_control", to_string(params.rate_control).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "target_bitrate", std::to_string(params.target_bitrate).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "key_frame_interval", std::to_string(params.key_frame_interval).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "frame_rate", std::to_string(params.frame_rate).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "output_chroma_format", to_string(params.output_chroma_format).c_str());
    ga_logger(Severity::INFO, "%s %s = 0x%x:0x%x\n", prefix.c_str(), "adapter_luid", params.adapter_luid.HighPart, params.adapter_luid.LowPart);
}

namespace {
    std::mutex g_mutex;
    std::condition_variable g_cv;
    bool g_stop = false;
}

static void signal_handler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stop  = true;
        printf("\nCTRL+C: user requested to stop pipeline.\n");
        g_cv.notify_one();
    }
}

int main(int argc, char* argv[]) {
    std::string bitrate = default_bitrate;
    std::string bitstream_frames_count = default_bitstream_frames_count;
    std::string codec = default_codec;
    std::string profile = default_profile;
    std::string display = default_display;
    std::string fps = default_fps;
    std::string gop = default_gop;
    std::string loglevel = default_loglevel;
    std::string rc = default_rc;

    int idx = 1;
    for (/* empty */; idx < argc; ++idx) {
        if (std::string("-h") == argv[idx] ||
            std::string("--help") == argv[idx]) {
            usage(argv[0]);
            exit(0);
        } else if (std::string("--bitrate") == argv[idx]) {
            if (++idx >= argc) break;
            bitrate = argv[idx];
        } else if (std::string("--codec") == argv[idx]) {
            if (++idx >= argc) break;
            codec = argv[idx];
        } else if (std::string("--profile") == argv[idx]) {
            if (++idx >= argc) break;
            profile = argv[idx];
        } else if (std::string("--display") == argv[idx]) {
            if (++idx >= argc) break;
            display = argv[idx];
        } else if (std::string("--fps") == argv[idx]) {
            if (++idx >= argc) break;
            fps = argv[idx];
        } else if (std::string("--gop") == argv[idx]) {
            if (++idx >= argc) break;
            gop = argv[idx];
        } else if (std::string("--loglevel") == argv[idx]) {
            if (++idx >= argc) break;
            loglevel = argv[idx];
        } else if (std::string("-n") == argv[idx]) {
            if (++idx >= argc) break;
            bitstream_frames_count = argv[idx];
        } else if (std::string("--rc") == argv[idx]) {
            if (++idx >= argc) break;
            rc = argv[idx];
        } else {
            break;
        }
    }

    if (idx >= argc) {
        fprintf(stderr, "fatal: invalid option or no output file specified\n");
        usage(argv[0]);
        return -1;
    }

    std::fstream bitstream_file(argv[idx], std::ios_base::out | std::ios_base::binary);
    if (bitstream_file.is_open() == false) {
        fprintf(stderr, "fatal: failed to open output bitstream\n");
        usage(argv[0]);
    }

    if (loglevel != std::string("none")) {
        ga_set_loglevel(ga_get_loglevel_enum(loglevel.c_str()));
        ga_conf_writev("logfile", "screen-grab-log.txt");
        ga_openlog();
    }

    // setting capture parameters
    DTCaptureParams capture_params;

    capture_params.output_format = DTCaptureParams::OutputFormat::rgb;

    HRESULT result = convert_utf8_to_utf16(display, capture_params.display_device_name);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": convert_utf8_to_utf16 failed for display device name\n");
        return E_FAIL;
    }

    int num_frames = 0;
    int target_bitstream_frames = -1;
    if (!arg_to_int(bitstream_frames_count, target_bitstream_frames) || (target_bitstream_frames < -1)) {
        fprintf(stderr, "fatal: unsupported bitstream frames count: %s\n", bitstream_frames_count.c_str());
        exit(1);
    }
    if (target_bitstream_frames == -1) target_bitstream_frames = INT_MAX;

    capture_params.on_packet_received = [&](const Packet& packet) {
        if (packet.data.empty())
            return;

        if (bitstream_file.is_open()) {
            bitstream_file.write(reinterpret_cast<const char*>(packet.data.data()), packet.data.size());
            // flush on keyframe
            if (packet.flags & Packet::flag_keyframe)
                bitstream_file.flush();
        }

        // count only till the limit to have correct final number of frames written to a file
        if (num_frames < target_bitstream_frames)
            ++num_frames;

        // stop capture after target encoded frame count is reached
        if (num_frames >= target_bitstream_frames) {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_stop = true;
            g_cv.notify_one();
        }
        if (num_frames % 100 == 0)
            printf("frames: %d\r", num_frames);
    };

    // [todo] implement cursor coordinates file dump
    capture_params.on_cursor_received = [](const CursorState&){};

    capture_params.on_error = [&](const std::string& msg, HRESULT res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        fprintf(stderr, "error: %s: 0x%08x\n", msg.c_str(), result);
        result = res;
        g_stop = true;
        g_cv.notify_one();
    };

    // setting encoder paramters
    EncoderParams encode_params;

    // codec
    if (ga_is_h264(codec))
        encode_params.codec = EncoderParams::Codec::avc;
    else if (ga_is_h265(codec))
        encode_params.codec = EncoderParams::Codec::hevc;
    else if (ga_is_av1(codec))
        encode_params.codec = EncoderParams::Codec::av1;
    else {
        fprintf(stderr, "fatal: unsupported codec: %s\n", codec.c_str());
        exit(1);
    }

    // profile
    if (profile != "unknown") {
        encode_params.profile = to_profile(encode_params.codec, profile);
        if (encode_params.profile == EncoderParams::Profile::unknown) {
            fprintf(stderr, "fatal: unsupported profile: %s\n", profile.c_str());
            exit(1);
        }
    }

    int target_bitrate = -1;
    if (!arg_to_int(bitrate, target_bitrate) || (target_bitrate <= 0)) {
        fprintf(stderr, "fatal: unsupported bitrate: %s\n", bitrate.c_str());
        exit(1);
    }
    encode_params.target_bitrate = static_cast<uint32_t>(target_bitrate);

    int target_fps = -1;
    if (!arg_to_int(fps, target_fps) || (target_fps <= 0)) {
        fprintf(stderr, "fatal: unsupported fps: %s\n", fps.c_str());
        exit(1);
    }
    encode_params.frame_rate = static_cast<uint32_t>(target_fps);

    int key_frame_interval = -1;
    if (!arg_to_int(gop, key_frame_interval) || (key_frame_interval <= 0)) {
        fprintf(stderr, "fatal: unsupported gop: %s\n", gop.c_str());
        exit(1);
    }
    encode_params.key_frame_interval = static_cast<uint32_t>(key_frame_interval);

    std::string rate_control = rc;
    if (rate_control == "cqp")
        encode_params.rate_control = EncoderParams::RateControl::cqp;
    else if (rate_control == "vbr")
        encode_params.rate_control = EncoderParams::RateControl::vbr;
    else {
        fprintf(stderr, "fatal: unsupported rate control: %s\n", rc.c_str());
        exit(1);
    }

    std::unique_ptr<DTCapture> capture_object = DTCapture::create(capture_params, encode_params);
    if (capture_object == nullptr) {
        fprintf(stderr, "fatal: failed to create capture object\n");
        exit(1);
    }

    log_capture_params(capture_params);
    log_encode_params(encode_params);
    printf("\n"); // step out from params printout

    result = capture_object->start();
    if (FAILED(result)) {
        fprintf(stderr, "fatal: failed to start capture\n");
        exit(1);
    }

    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, []{ return g_stop; });
    }
    printf("frames: %d\n", num_frames);

    capture_object->stop();

    if (FAILED(result))
        return 1;
    return 0;
}
