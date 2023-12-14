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

#include "telemetry-manager.h"

#include "encoder-common.h" // needed for FrameMetaData struct
#include "QoSMgt.h" // needed for QosInfo struct

TelemetryManager::~TelemetryManager() {
    if (m_frame_stats_file.is_open())
        m_frame_stats_file.close();
    if (m_client_stats_file.is_open())
        m_client_stats_file.close();
}

std::unique_ptr<TelemetryManager> TelemetryManager::create(const TelemetryManagerParams& params) {
    auto instance = std::unique_ptr<TelemetryManager>(new TelemetryManager);
    instance->m_params = params;

    if (!params.frame_statistics_filename.empty()) {
        instance->m_frame_stats_file.open(params.frame_statistics_filename, std::ios_base::out | std::ios_base::binary);
        if (instance->m_frame_stats_file.is_open() == false) {
            ga_logger(Severity::ERR, __FUNCTION__ ": failed to open video statistics file\n");
            return nullptr;
        }

        // write header
        instance->m_frame_stats_file << "frame_no"
            << ",encoded_frame_size(bytes)"
            << ",key_frame"
            // timestamps
            << ",capture_start_ts(us)"
            << ",capture_end_ts(us)"
            << ",encode_start_ts(us)"
            << ",encode_end_ts(us)"
            << ",presentation_ts(us)"
            // op duration
            << ",capture_time(us)"
            << ",capture_interval(us)"
            << ",encode_time(us)"
            << ",encode_interval(us)"
            << ",frame_delay(us)"
            // average stats over 'max_frames'
            << ",average_capture_fps_" << m_max_frames << "_frames"
            << ",average_encode_fps_" << m_max_frames << "_frames"
            << ",average_bitrate_" << m_max_frames << "_frames" << "(kbps)"
            << "\n";
    }

    if (!params.client_statistics_filename.empty()) {
        instance->m_client_stats_file.open(params.client_statistics_filename, std::ios_base::out | std::ios_base::binary);
        if (instance->m_client_stats_file.is_open() == false) {
            ga_logger(Severity::ERR, __FUNCTION__ ": failed to open video statistics file\n");
            return nullptr;
        }

        // write header
        instance->m_client_stats_file << "frame_ts"
            << "," << "frame_delay"
            << "," << "frame_size"
            << "," << "packet_loss"
            << "\n";
    }

    // reset context
    instance->m_frame_count = 0;
    instance->m_stats_queue.clear();
    return instance;
}

void TelemetryManager::update_frame_statistics(const Packet& packet) {
    std::lock_guard lk(m_frame_stats_lock);

    using namespace std::chrono;
    using namespace std::chrono_literals;

    const auto& ti = packet.timing_info;

    FrameStatistics stats = {};
    stats.capture_start_ts = ti.capture_start_ts;
    stats.capture_end_ts = ti.capture_end_ts;
    stats.encode_start_ts = ti.encode_start_ts;
    stats.encode_end_ts = ti.encode_end_ts;
    stats.presentation_ts = ti.presentation_ts;
    stats.frame_size = packet.data.size();
    stats.frame_num = m_frame_count;
    if (packet.flags & Packet::flag_keyframe)
        stats.key_frame = 1;

    m_frame_count++;

    // dump to file
    dump_frame_statistics(stats);

    // update buffer
    // remove old frame from buffer if max window size is reached
    if (m_stats_queue.size() >= m_max_frames)
        m_stats_queue.pop_front();
    m_stats_queue.push_back(stats);
}

void TelemetryManager::frame_statistics(FrameStatistics& stats) const {
    std::lock_guard lk(m_frame_stats_lock);
    if (!m_stats_queue.empty())
        stats = m_stats_queue.back();
}

void TelemetryManager::update_client_timestamp(const time_point_t& timestamp) {
    std::lock_guard lk(m_client_stats_lock);
    m_client_timestamp = timestamp;
}

TelemetryManager::time_point_t TelemetryManager::client_timestamp() {
    std::lock_guard lk(m_client_stats_lock);
    return m_client_timestamp;
}

void TelemetryManager::update_client_statistics(const ClientStatistics& stats) {
    std::lock_guard lk(m_client_stats_lock);
    dump_client_statistics(stats);
}

void TelemetryManager::dump_frame_statistics(const FrameStatistics& stats) {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    if (!m_frame_stats_file.is_open())
        return;

    // frame order and size
    int64_t frame_num = stats.frame_num;
    int64_t frame_size = stats.frame_size;
    int64_t key_frame = stats.key_frame;
    // timestamps in us
    int64_t capture_start_us = duration_cast<microseconds>(stats.capture_start_ts.time_since_epoch()).count();
    int64_t capture_end_us = duration_cast<microseconds>(stats.capture_end_ts.time_since_epoch()).count();
    int64_t encode_start_us = duration_cast<microseconds>(stats.encode_start_ts.time_since_epoch()).count();
    int64_t encode_end_us = duration_cast<microseconds>(stats.encode_end_ts.time_since_epoch()).count();
    int64_t presentation_us = duration_cast<microseconds>(stats.presentation_ts.time_since_epoch()).count();
    // operation time
    int64_t capture_time = capture_end_us - capture_start_us;
    int64_t capture_interval = 0;
    int64_t encode_time = encode_end_us - encode_start_us;
    int64_t encode_interval = 0;
    int64_t frame_delay = encode_end_us - capture_start_us;

    // average stats over max_frames
    double avg_capture_fps = 0;
    double avg_encode_fps = 0;
    int64_t avg_bitrate = 0;
    if (!m_stats_queue.empty()) {
        int64_t prev_capture_start_us = duration_cast<microseconds>(m_stats_queue.back().capture_start_ts.time_since_epoch()).count();
        int64_t prev_encode_end_us = duration_cast<microseconds>(m_stats_queue.back().encode_end_ts.time_since_epoch()).count();
        capture_interval = capture_start_us - prev_capture_start_us;
        encode_interval = encode_end_us - prev_encode_end_us;

        int64_t oldest_capture_start_us = duration_cast<microseconds>(m_stats_queue.front().capture_start_ts.time_since_epoch()).count();
        int64_t oldest_encode_end_us = duration_cast<microseconds>(m_stats_queue.front().encode_end_ts.time_since_epoch()).count();
        int64_t capture_time_total = capture_start_us - oldest_capture_start_us;
        int64_t encode_time_total = encode_end_us - oldest_encode_end_us;
        int64_t capture_time_avg = capture_time_total / m_stats_queue.size();
        int64_t encode_time_avg = encode_time_total / m_stats_queue.size();

        avg_capture_fps = (capture_time_avg != 0) ? double(microseconds(1s).count()) / double(capture_time_avg) : 0.;
        avg_encode_fps = (encode_time_avg != 0) ? double(microseconds(1s).count()) / double(encode_time_avg) : 0.;

        avg_bitrate = frame_size;
        for (auto& frame : m_stats_queue)
            avg_bitrate += frame.frame_size;
        // exclude oldest frame
        avg_bitrate -= m_stats_queue.front().frame_size;
        // average bitrate
        avg_bitrate = (encode_time_total != 0) ? avg_bitrate * microseconds(1s).count() / encode_time_total : 0; // bytes/s
        avg_bitrate = (avg_bitrate * 8 + 999) / 1000; // to kbps
    }

    m_frame_stats_file << stats.frame_num
        << "," << stats.frame_size
        << "," << stats.key_frame
        // timestamps
        << "," << capture_start_us
        << "," << capture_end_us
        << "," << encode_start_us
        << "," << encode_end_us
        << "," << presentation_us
        // op duration
        << "," << capture_time
        << "," << capture_interval
        << "," << encode_time
        << "," << encode_interval
        << "," << frame_delay
        // average stats over 'max_frames'
        << "," << avg_capture_fps
        << "," << avg_encode_fps
        << "," << avg_bitrate
        << "\n";
}

void TelemetryManager::dump_client_statistics(const ClientStatistics& stats) {
    if (!m_client_stats_file.is_open())
        return;
    
    m_client_stats_file << stats.frame_ts
        << "," << stats.frame_delay
        << "," << stats.frame_size
        << "," << stats.packet_loss
        << "\n";
}
