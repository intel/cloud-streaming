// Copyright (C) 2018-2022 Intel Corporation
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

#ifndef CIRRVIDEODEMUX_H
#define CIRRVIDEODEMUX_H

#include <mutex>
#include <condition_variable>
#include "api/irrv.h"

extern "C" {
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}
#include "CDemux.h"
#include "utils/ProfTimer.h"
#include "utils/IORuntimeWriter.h"
#include "utils/TimeLog.h"
#include <map>
#include <list>

#define NEW_FRAME_WAIT_TIMEOUT_MCS 1000000 //1s

class CIrrVideoDemux : public CDemux {
public:
    CIrrVideoDemux(int w, int h, int format, float framerate, IrrPacket* pkt);
    ~CIrrVideoDemux();
    CIrrVideoDemux(const CIrrVideoDemux& orig) = delete;
    CIrrVideoDemux &operator= (const CIrrVideoDemux&) = delete;

    int getNumStreams();
    CStreamInfo* getStreamInfo(int strIdx);
    int readPacket(IrrPacket *pkt);
    int sendPacket(IrrPacket *pkt);

    int setLatencyStats(int nLatencyStats);

    void setRuntimeWriter(IORuntimeWriter::Ptr writer) { mRuntimeWriter = std::move(writer); }

    void updateDynamicChangedFramerate(int framerate);

    void stop();

private:
    std::mutex                  m_Lock;
    std::condition_variable     m_cv;
    CStreamInfo                 m_Info;
    IrrPacket                   m_Pkt;
    int64_t                     m_nPrevPts;
    int64_t                     m_totalWaitMcs;
    IORuntimeWriter::Ptr        mRuntimeWriter;

    ///< pkt_round: profile one round, mostly it is 1/fps, such as 40ms for 25fps
    ///< pkt_latency: profile the latency the interval start
    ///< from (3D send pakcet to demux) to (CTransCoder read packet from demux)
    std::map<std::string, ProfTimer *> m_mProfTimer;
    int m_nLatencyStats;
    bool m_bStartLatency;

    bool m_stop;
    volatile bool m_notified;

    std::unique_ptr<CTransLog> m_logger;
};

#endif /* CIRRVIDEODEMUX_H */

