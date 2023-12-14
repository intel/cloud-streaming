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

#include "encoder.h"

#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>

struct TelemetryManagerParams {
    std::filesystem::path frame_statistics_filename;
    std::filesystem::path client_statistics_filename;
};

class TelemetryManager {
public:
    using duration_t = FrameTimingInfo::duration_t;
    using time_point_t = FrameTimingInfo::time_point_t;

    struct FrameStatistics {
        // video capture timestamps
        time_point_t capture_start_ts;
        time_point_t capture_end_ts;
        // encode timestamps
        time_point_t encode_start_ts;
        time_point_t encode_end_ts;
        // presentation timestamp
        time_point_t presentation_ts;
        // frame info
        uint32_t frame_size;
        uint32_t frame_num;
        int32_t key_frame;
    };

    struct ClientStatistics {
        int64_t frame_ts;
        int64_t frame_size;
        int64_t frame_delay;
        int64_t frame_start_delay;
        int64_t packet_loss;
    };

    virtual ~TelemetryManager();

    /**
     * @brief      Create telemetry manager instance
     *
     * @param[in]  params  telemetry manager parameters
     *
     * @return     new instance, on success
     *             nullptr, on error
     */
    static std::unique_ptr<TelemetryManager> create(const TelemetryManagerParams& params);

    /**
     * @brief      Update internal frame statistics
     *
     * @param[in]  packet  encoded frame packet
     */
    void update_frame_statistics(const Packet& packet);

    /**
     * @brief      Get statistics for last frame
     *
     * @param      stats  frame statistics
     */
    void frame_statistics(FrameStatistics& stats) const;

    /**
     * @brief      Update received client timestamp
     *
     * @param[in]  timestamp  timestamp
     */
    void update_client_timestamp(const time_point_t& timestamp);

    /**
     * @return     Return last signalled client timestamp
     */
    time_point_t client_timestamp();

    /**
     * @brief      Update internal client statistics
     *
     * @param[in]  stats  client statistics
     */
    void update_client_statistics(const ClientStatistics& stats);

private:
    TelemetryManager() = default;

    void dump_frame_statistics(const FrameStatistics& stats);

    void dump_client_statistics(const ClientStatistics& stats);

private:
    TelemetryManagerParams m_params;

    std::ofstream m_frame_stats_file;
    std::ofstream m_client_stats_file;

    // moving average window size
    static constexpr uint32_t m_max_frames = 100;

    mutable std::mutex m_frame_stats_lock;
    int64_t m_frame_count = 0;
    std::deque<FrameStatistics> m_stats_queue;

    mutable std::mutex m_client_stats_lock;
    time_point_t m_client_timestamp;
};
