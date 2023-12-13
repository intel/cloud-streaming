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
#include "ga-module.h"
#include "encoder-common.h" // libga: needed FrameMetaData struct

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "dt-capture.h"
#include "cursor-sender.h"

class BitstreamWriter; // forward decl

namespace {
    std::unique_ptr<DTCapture> capture_object;
    std::unique_ptr<CursorSender> cursor_sender;
    std::unique_ptr<BitstreamWriter> bitstream_writer;
} // namespace

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

struct BitstreamWriterParams {
    std::filesystem::path bitstream_filename;
    int32_t max_frames = -1;
};

class BitstreamWriter {
public:
    virtual ~BitstreamWriter() = default;

    static std::unique_ptr<BitstreamWriter> create(const BitstreamWriterParams& params) {
        auto instance = std::unique_ptr<BitstreamWriter>(new BitstreamWriter);

        instance->m_params = params;
        instance->m_file.open(params.bitstream_filename, std::ios_base::out | std::ios_base::binary);
        if (instance->m_file.is_open() == false) {
            ga_logger(Severity::ERR, __FUNCTION__ ": failed to open output bitstream dump file\n");
            return nullptr;
        }

        return instance;
    }

    void write_packet(const Packet& packet) {
        if (m_file.is_open() && (m_params.max_frames < 0 || m_count < m_params.max_frames)) {
            m_file.write(reinterpret_cast<const char*>(packet.data.data()), packet.data.size());
            // flush on keyframe
            if (packet.flags & Packet::flag_keyframe)
                m_file.flush();

            m_count++;
        }
    }

private:
    BitstreamWriter() = default;

    BitstreamWriterParams m_params;
    std::ofstream m_file;
    int32_t m_count = 0;
};

static void send_packet(const Packet& packet) {
    if (packet.data.empty())
        return; // no data to send - return

    // write packet to file
    if (bitstream_writer)
        bitstream_writer->write_packet(packet);

    // create ga_packet
    ga_packet_t pkt;

    // handling frame-data: send all in one shot
    ga_init_packet(&pkt);
    pkt.data = const_cast<uint8_t*>(packet.data.data()); // [todo] remove const cast
    pkt.pts = 0;
    pkt.size = packet.data.size();
    pkt.flags = (packet.flags & Packet::flag_keyframe)
        ? GA_PKT_FLAG_KEY
        : 0;

    // allocate side data
    uint8_t* pkt_side_data = ga_packet_new_side_data(&pkt, ga_packet_side_data_type::GA_PACKET_DATA_NEW_EXTRADATA, sizeof(FrameMetaData));
    if (pkt_side_data == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ga_packet_new_side_data failed\n");
        return;
    }

    FrameMetaData* pkt_meta_data = reinterpret_cast<FrameMetaData*>(pkt_side_data);

    // [todo] telemetry support
    pkt_meta_data->last_slice = true;
    pkt_meta_data->capture_time_ms = 0;
    pkt_meta_data->encode_start_ms = 0;
    pkt_meta_data->encode_end_ms = 0;
#ifdef E2ELATENCY_TELEMETRY_ENABLED
    pkt_meta_data->latency_msg_size = 0;
    pkt_meta_data->latency_msg_data = nullptr;
#endif

    // packet timestamp
    // [fixme] should be encoder timestamp
    struct timeval pkttv;
    gettimeofday(&pkttv, nullptr);

    // send packet over network
    auto send_result = encoder_send_packet("video-encoder", 0, &pkt, pkt.pts, &pkttv);

    // free side data
    ga_packet_free_side_data(&pkt);

    // check send result
    if (send_result < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": encoder_send_packet failed\n");
        return;
    }
}

static HRESULT setup_capture_params(DTCaptureParams& params) {
    params = {};

    // display device name
    std::string display_device_name_utf8 = ga_conf_readstr("display");
    HRESULT result = convert_utf8_to_utf16(display_device_name_utf8, params.display_device_name);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": convert_utf8_to_utf16 failed for display device name\n");
        return E_FAIL;
    }

    // output format
    bool use_rgb_output = ga_conf_readbool("encoder-rgb-mode", 0);
    params.output_format = use_rgb_output
        ? DTCaptureParams::OutputFormat::rgb
        : DTCaptureParams::OutputFormat::nv12;

    // set callbacks to send packet and cursor downstream to webrtc module
    params.on_packet_received = send_packet;
    params.on_cursor_received = [&](const CursorState& state) { cursor_sender->update_cursor(state); };

    return S_OK;
}

static HRESULT setup_encode_params(EncoderParams& params) {
    params = {};

    // codec
    std::string codec = ga_conf_readstr("video-codec");
    if (ga_is_h264(codec))
        params.codec = EncoderParams::Codec::avc;
    else if (ga_is_h265(codec))
        params.codec = EncoderParams::Codec::hevc;
    else if (ga_is_av1(codec))
        params.codec = EncoderParams::Codec::av1;
    else {
        // video-codec is not set -> use AVC by default
        ga_logger(Severity::WARNING, __FUNCTION__ ": video-codec is not set, selecting AVC/H264 codec\n");
        params.codec = EncoderParams::Codec::avc;
    }

    // profile
    std::string pix_fmt = ga_conf_readstr("pix_fmt");
    if ((params.codec == EncoderParams::Codec::hevc) && (pix_fmt == std::string("yuv444p"))) {
        params.profile = EncoderParams::Profile::hevc_rext;
    }

    // target bitrate
    int target_bitrate = ga_conf_mapreadint("video-specific", "b");
    if (target_bitrate <= 0) {
        // set default target-bitrate
        target_bitrate = 3000000;
        ga_logger(Severity::WARNING, __FUNCTION__ ": video-bitrate is not set, setting video-bitrate to %d bps\n", target_bitrate);
    }
    params.target_bitrate = static_cast<uint32_t>(target_bitrate);

    // encode fps
    int video_fps = ga_conf_readint("video-fps");
    if (video_fps <= 0) {
        // set default video-fps
        video_fps = 60;
        ga_logger(Severity::WARNING, __FUNCTION__ ": video-fps is not set, setting video-fps to %d\n", video_fps);
    }
    params.frame_rate = video_fps;

    // key frame interval
    int key_frame_interval = ga_conf_mapreadint("video-specific", "g");
    if (key_frame_interval <= 0) {
        // set default key-frame-interval to 1s
        key_frame_interval = video_fps;
        ga_logger(Severity::WARNING, __FUNCTION__ ": key-frame-interval is not set, setting key-frame-interval to %d\n", key_frame_interval);
    }
    params.key_frame_interval = static_cast<uint32_t>(key_frame_interval);

    // rate control method
    std::string rate_control = ga_conf_readstr("video-rc");
    if (rate_control == "cqp")
        params.rate_control = EncoderParams::RateControl::cqp;
    else if (rate_control == "vbr")
        params.rate_control = EncoderParams::RateControl::vbr;
    else {
        ga_logger(Severity::WARNING, __FUNCTION__ ": video-rc is not set, setting rate cotrol method to 'vbr'\n");
        params.rate_control = EncoderParams::RateControl::vbr;
    }

#if 0 // [todo] tcae support
    if (params.rate_control == EncoderParams::RateControl::VBR) {
        /* enable tcae by default for vbr */
    }
#endif

    // [todo] remove deprecated [slice-based-encoding] option from conf file
    // [todo] remove deprecated [gpu-based-sync] option from conf file
    // [fixme] enable [pix_fmt] for 444 // ga_conf_readstr("pix_fmt");
    // frame rate control
    int enable_frc = ga_conf_readbool("enable-frc", 0);
    if (enable_frc <= 0) {
        enable_frc = 1;
        ga_logger(Severity::WARNING, __FUNCTION__ ": enable-frc is not set, frame rate control is enabled by default\n");
    }

#if 0 // [todo] tcae support
    int enable_tcae = ga_conf_readbool("enable-tcae", 0);
    if (enable_tcae > 0) {
        int tcae_target_delay = ga_conf_readint("netpred-target-delay");
        int tcae_netpred_records = ga_conf_readint("netpred-records");
        int tcae_log = ga_conf_readbool("dump-debugstats", false);
    }
#endif

    return S_OK;
}

static HRESULT setup_bitstream_dump_config(BitstreamWriterParams& params) {
    params = {};

    constexpr size_t max_path_len = 32768;
    std::wstring default_dump_location = L"C:\\temp";

    // encoder output dump
    std::wstring bitstream_filename;
    bool dump_bitstream = ga_conf_readbool("enable-bs-dump", 0);
    if (dump_bitstream) {
        std::string filename_utf8(max_path_len, '\0');
        auto ptr = ga_conf_readv("video-bs-file", filename_utf8.data(), filename_utf8.size());
        if (ptr != nullptr) {
            HRESULT result = convert_utf8_to_utf16(filename_utf8, bitstream_filename);
            if (FAILED(result)) {
                ga_logger(Severity::ERR, __FUNCTION__ ": convert_utf8_to_utf16 failed for bitstream filename\n");
                return E_FAIL;
            }
        }
    }

    if (dump_bitstream) {
        if (bitstream_filename.empty()) {
            auto pid = GetCurrentProcessId();
            std::wstring filename_suffix;

            std::string codec = ga_conf_readstr("video-codec");
            if (ga_is_h264(codec))
                filename_suffix = L".h264";
            else if (ga_is_h265(codec))
                filename_suffix = L".h265";
            else if (ga_is_av1(codec))
                filename_suffix = L".av1";
            else
                filename_suffix = L".bs";

            bitstream_filename = default_dump_location
                + L"\\bitstream_" + std::to_wstring(pid) + filename_suffix;
        }
        params.bitstream_filename = bitstream_filename;
        params.max_frames = -1; // infinite
    }

    return S_OK;
}

static std::string to_string(DTCaptureParams::OutputFormat format) {
    switch (format) {
    case DTCaptureParams::OutputFormat::rgb:
        return "rgb";
    case DTCaptureParams::OutputFormat::nv12:
        return "nv12";
    }
    return "unknown";
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
    fprintf(stdout, "%s %s = 0x%x:0x%x\n", prefix.c_str(), "adapter_luid", params.adapter_luid.HighPart, params.adapter_luid.LowPart);

    ga_logger(Severity::INFO, "%s --- encode config:\n", prefix.c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "codec", to_string(params.codec).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "profile", to_string(params.codec, params.profile).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "preset", to_string(params.preset).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "rate_control", to_string(params.rate_control).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "target_bitrate", std::to_string(params.target_bitrate).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "key_frame_interval", std::to_string(params.key_frame_interval).c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "frame_rate", std::to_string(params.frame_rate).c_str());
    ga_logger(Severity::INFO, "%s %s = 0x%x:0x%x\n", prefix.c_str(), "adapter_luid", params.adapter_luid.HighPart, params.adapter_luid.LowPart);
}

static void log_bitstream_dump_config(const BitstreamWriterParams& params) {
    const std::string prefix = "desktop-capture:";

    bool enabled = !params.bitstream_filename.empty();

    fprintf(stdout, "%s --- bitstream dump config:\n", prefix.c_str());
    fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "dump_bitstream", (enabled ? "yes" : "no"));
    if (enabled)
        fprintf(stdout, "%s %s = %s\n", prefix.c_str(), "bitstream_filename", params.bitstream_filename.string().c_str());

    ga_logger(Severity::INFO, "%s --- bitstream dump config:\n", prefix.c_str());
    ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "dump_bitstream", (enabled ? "yes" : "no"));
    if (enabled)
        ga_logger(Severity::INFO, "%s %s = %s\n", prefix.c_str(), "bitstream_filename", params.bitstream_filename.string().c_str());
}

static int desktop_capture_init(void *arg, void(*p)(struct timeval)) {
    ga_logger(Severity::INFO, "desktop-capture : module init\n");

    if (capture_object != nullptr) {
        ga_logger(Severity::WARNING, "desktop-capture : module is already initialized\n");
        return 0; // already initialized
    }

    HRESULT result = S_OK;

    DTCaptureParams capture_params;
    result = setup_capture_params(capture_params);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": setup_capture_config failed, result = 0x%08x\n", result);
        return -1;
    }

    EncoderParams encode_params;
    result = setup_encode_params(encode_params);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": setup_encode_config failed, result = 0x%08x\n", result);
        return -1;
    }

    capture_object = DTCapture::create(capture_params, encode_params);
    if (capture_object == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": DTCapture->create() failed\n");
        return -1;
    }

    cursor_sender = CursorSender::create();
    if (cursor_sender == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": CursorSender->create() failed\n");
        return -1;
    }

    log_capture_params(capture_params);
    log_encode_params(encode_params);

    BitstreamWriterParams bitstream_writer_params;
    result = setup_bitstream_dump_config(bitstream_writer_params);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": setup_bitstream_dump_config failed, result = 0x%08x\n", result);
        return -1;
    }

    if (!bitstream_writer_params.bitstream_filename.empty()) {
        bitstream_writer = BitstreamWriter::create(bitstream_writer_params);
        if (bitstream_writer == nullptr) {
            ga_logger(Severity::ERR, __FUNCTION__ ": BitstreamWriter->create() failed\n");
            return -1;
        }
    }

    log_bitstream_dump_config(bitstream_writer_params);

    return 0;
}

static int desktop_capture_start(void *arg) {
    ga_logger(Severity::INFO, "desktop-capture : module start\n");

    if (capture_object == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": capture is not initialized\n");
        return -1;
    }

    HRESULT result = capture_object->start();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": DTCapture->start() failed, result = 0x%08x\n", result);
        return -1;
    }

    ga_logger(Severity::INFO, "desktop-capture : module started\n");
    return 0;
}

static int desktop_capture_stop(void *arg) {
    ga_logger(Severity::INFO, "desktop-capture : module stop\n");

    if (capture_object)
        capture_object->stop();

    ga_logger(Severity::INFO, "desktop-capture : module stopped\n");
    return 0;
}

static int desktop_capture_ioctl(int command, int argsize, void *arg) {
    int result = GA_IOCTL_ERR_NONE;
    switch (command) {
    case GA_IOCTL_REQUEST_KEYFRAME:
        ga_logger(Severity::INFO, "desktop-capture : key frame requested\n");
        if (capture_object) {
            capture_object->on_key_frame_request();
        }
        if (cursor_sender) {
            // key frame request implies client is connected
            cursor_sender->on_client_connect();
        }
        break;
    case GA_IOCTL_PAUSE:
        ga_logger(Severity::INFO, "desktop-capture : client disconnected\n");
        if (cursor_sender)
            cursor_sender->on_client_disconnect();
        break;
    case GA_IOCTL_REQUEST_NEW_CURSOR:
        ga_logger(Severity::INFO, "desktop-capture : new cursor requested\n");
        if (cursor_sender)
            cursor_sender->on_resend_cursor();
        break;
    case GA_IOCTL_UPDATE_CLIENT_EVENT:
    case GA_IOCTL_UPDATE_FRAME_STATS:
    case GA_IOCTL_SET_MAX_BPS:
        /* do nothing */
        break;
    default:
        result = GA_IOCTL_ERR_NOTSUPPORTED;
        break;
    }
    return result;
}

static int desktop_capture_release(void *arg) {
    ga_logger(Severity::INFO, "desktop-capture : module release\n");

    capture_object.reset();
    cursor_sender.reset();
    bitstream_writer.reset();

    ga_logger(Severity::INFO, "desktop-capture : module release\n");
    return 0;
}

ga_module_t* module_load() {
    static ga_module_t m;
    memset(&m, 0, sizeof(ga_module_t));
    m.type = GA_MODULE_TYPE_VENCODER;
    m.name = "intel-video-encoder";
    m.mimetype = "video/H264";
    m.init = desktop_capture_init;
    m.start = desktop_capture_start;
    m.stop = desktop_capture_stop;
    m.deinit = desktop_capture_release;
    m.ioctl = desktop_capture_ioctl;
    return &m;
}
