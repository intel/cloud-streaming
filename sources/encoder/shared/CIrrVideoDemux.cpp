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

#include <algorithm>
#include <chrono>
#include "CIrrVideoDemux.h"
#include "IrrStreamer.h"
#include "utils/CTransLog.h"

#ifdef __DEBUG
#define DEBUG_LOG(...)  do {                                                                           \
        m_logger->Debug("%s: %d :: TimeStamp = %ld: ", __FUNCTION__, __LINE__, av_gettime_relative()); \
        m_logger->Debug(__VA_ARGS__);                                                                  \
        m_logger->Debug("\n");                                                                         \
    } while(0);
#else
#define DEBUG_LOG(...) ;
#endif

CIrrVideoDemux::CIrrVideoDemux(int w, int h, int format, float framerate, IrrPacket* pkt) :m_Lock(), m_cv() {
    m_logger = std::move(std::unique_ptr<CTransLog>(new CTransLog("CIrrVideoDemux::")));

    m_Info.m_pCodecPars->codec_type = AVMEDIA_TYPE_VIDEO;
    m_Info.m_pCodecPars->codec_id   = AV_CODEC_ID_RAWVIDEO;
    m_Info.m_pCodecPars->format     = format;
    m_Info.m_pCodecPars->width      = w;
    m_Info.m_pCodecPars->height     = h;
    m_Info.m_rFrameRate             = av_d2q(framerate, 1024);
    m_Info.m_rTimeBase              = AV_TIME_BASE_Q;
    m_nPrevPts                      = 0;
    m_totalWaitMcs                  = 0;

    av_packet_move_ref(&m_Pkt.av_pkt, &pkt->av_pkt);
    if (pkt->display_ctrl != nullptr) {
        m_logger->Warn("pkt->display_ctrl expected to be nullptr\n");
    }

    // Latency
    m_nLatencyStats = 0;
    m_bStartLatency = false;

    m_stop = false;
    m_notified = false;
}

CIrrVideoDemux::~CIrrVideoDemux() {
    av_packet_unref(&m_Pkt.av_pkt);
}


int CIrrVideoDemux::getNumStreams() {
    return 1;
}

CStreamInfo* CIrrVideoDemux::getStreamInfo(int strIdx) {
    return &m_Info;
}

void CIrrVideoDemux::updateDynamicChangedFramerate(int framerate) {
    m_Info.m_rFrameRate = (AVRational){framerate, 1};
}

void CIrrVideoDemux::stop()
{
    DEBUG_LOG("Entry. Pre-Lock Acquire");

    {
        std::unique_lock<std::mutex> lock(m_Lock);
        m_stop = true;
    }

    DEBUG_LOG("Lock Released");
    m_cv.notify_one();
}

int CIrrVideoDemux::readPacket(IrrPacket *irrpkt) {

    // This function is called in the Encoding thread, and "Posts" frames for encode
    // when appropriate. Two scenarios when the Post event occurs.
    // 1) A new frame notification by the thread that receives frames from HWC (renderFpsEnc = 1 mode)
    // 2) A time-window corresponding to target fps elapses (renderFpsEnc = 0 mode)

    int ret = 0;

    TimeLog timelog("IRRB_CIrrVideoDemux_readPacket");
    ATRACE_CALL();

    // Thread control variables
    std::unique_lock<std::mutex> lock(m_Lock);
    std::chrono::microseconds time_to_wait(1000000);
    bool notify_status = false;

    // Track Time Window for single frame assuming constant fps
    int64_t frame_mcs = av_rescale_q(1, av_inv_q(m_Info.m_rFrameRate), m_Info.m_rTimeBase);
    int64_t curr_mcs  = av_gettime_relative();

    if (!getRenderFpsEncFlag()) {

        int64_t time_since_last_post = curr_mcs - m_nPrevPts;
        int64_t wait_mcs = std::max<int64_t>(frame_mcs - time_since_last_post, 0);
        time_to_wait = std::chrono::microseconds(wait_mcs);

        DEBUG_LOG("curr_mcs = %ld, m_nPrevPts = %ld, wait_mcs = %ld, frame_mcs = %ld, "
                  "time_since_last_post = %ld",
                  curr_mcs, m_nPrevPts, wait_mcs, frame_mcs, time_since_last_post);


        if (wait_mcs > 0) {
            // Release lock before threads sleeps to account for wait time (to maintain fps)
            // If SendPacket is called more than once before this thread is active again, m_pkt will be
            // overwritten by the AiC frame in the latest call (older ones are effectively dropped
            // without submitting to the encoder)
            lock.unlock();
            std::this_thread::sleep_for(time_to_wait);
            m_totalWaitMcs += wait_mcs;

            // Re-Acquire lock and continue
            lock.lock();
        }

        notify_status = m_notified;
    }
    else { //RenderFpsEncFlag = 1

        // Follow Render fps instead of fixed encode fps.
        // If configured, adhere to a min fps

        int64_t time_wait = NEW_FRAME_WAIT_TIMEOUT_MCS;
        if (getMinFpsEnc() != 0)
            time_wait = 1000000 / getMinFpsEnc();
        time_to_wait = std::chrono::microseconds(time_wait);

        //Wait for notification, till time-out
        notify_status = m_cv.wait_for(lock, time_to_wait,  [&] { return m_notified; });

        m_totalWaitMcs += time_wait;
    }

    if (notify_status || m_stop) {
        //Reset wait time counter if there is a frame notified
        m_totalWaitMcs = 0;
    }

    DEBUG_LOG("status = %d, time_to_wait = %ld, m_stop = %d, m_totalWaitMcs = %ld",
              (int)notify_status, time_to_wait, (int)m_stop, m_totalWaitMcs);

    if (m_totalWaitMcs >= NEW_FRAME_WAIT_TIMEOUT_MCS) {
        // Reset wait time measurement
        m_totalWaitMcs = 0;

        m_logger->Debug("ReadPacket: No new frame notification for last 1s");
    }

    // Check if a valid packet is received. Exit with INVALIDDATA error if not
    if (!m_Pkt.av_pkt.buf || !m_Pkt.av_pkt.buf->data) {
        if (!m_Pkt.av_pkt.buf) {
            m_logger->Error("ReadPacket: m_Pkt.av_pkt.buf (AVBufferRef* from pool) is NULL!\n");
        }
        else if (!m_Pkt.av_pkt.buf->data) {
            m_logger->Error("ReadPacket: m_Pkt.av_pkt.buf->data (mfxFrameSurface1*) is NULL!\n");
        }
        ret = AVERROR_INVALIDDATA;
        goto cleanup;
    }

    // Latency stats book-keeping
    if (m_nLatencyStats && m_bStartLatency) {
        if(m_Pkt.av_pkt.pts!=AV_NOPTS_VALUE){
            m_mProfTimer["pkt_latency"]->profTimerEnd("pkt_latency");
        }
    }

    // Copy received packet from shared resource m_Pkt
    ret = av_packet_ref(&irrpkt->av_pkt, &m_Pkt.av_pkt);
    irrpkt->display_ctrl = std::move(m_Pkt.display_ctrl);

    // Runtime Dump input
    if (mRuntimeWriter && mRuntimeWriter->getRuntimeWriterStatus() != RUNTIME_WRITER_STATUS::STOPPED) {
        auto pkt_data = std::make_shared<IORuntimeData>();
        if (CDemux::getVASurfaceFlag()) {
            uint32_t surfaceId;
            memcpy(&surfaceId, irrpkt->av_pkt.data, sizeof(uint32_t));
            pkt_data->va_surface_id = surfaceId;
            pkt_data->type = IORuntimeDataType::VAAPI_SURFACE;
        } else {
            pkt_data->data = irrpkt->av_pkt.data;
            pkt_data->size = irrpkt->av_pkt.size;
            pkt_data->type = IORuntimeDataType::SYSTEM_BLOCK;
        }
        pkt_data->width = m_Info.m_pCodecPars->width;
        pkt_data->height = m_Info.m_pCodecPars->height;
        pkt_data->format = IORuntimeWriter::avFormatToFourCC(m_Info.m_pCodecPars->format);

        mRuntimeWriter->submitRuntimeData(RUNTIME_WRITE_MODE::INPUT, std::move(pkt_data));
    }

    if (m_nLatencyStats && (m_nPrevPts>0)) {
        m_mProfTimer["pkt_round"]->profTimerEnd("pkt_round", m_nPrevPts);
    }

    irrpkt->av_pkt.pts = irrpkt->av_pkt.dts = m_nPrevPts = av_gettime_relative();

    if (m_nLatencyStats && m_bStartLatency ) {
        m_Pkt.av_pkt.pts = AV_NOPTS_VALUE;
    }

cleanup:
    m_notified = false;
    return ret;
}

int CIrrVideoDemux::sendPacket(IrrPacket *pkt) {
    DEBUG_LOG("Entry. Pre-Lock Acquire");

    TimeLog timelog("IRRB_CIrrVideoDemux_sendPacket");
    ATRACE_CALL();

    {
        std::unique_lock<std::mutex> lock(m_Lock);
        DEBUG_LOG("Lock Acquired");

        av_packet_unref(&m_Pkt.av_pkt);
        av_packet_move_ref(&m_Pkt.av_pkt, &pkt->av_pkt);
        // if m_Pkt.display_ctrl is not nullptr, the ctrl is not read. Keep it non-nullptr
        // to avoid missing ctrl SEI.
        if (pkt->display_ctrl != nullptr)
            m_Pkt.display_ctrl = std::move(pkt->display_ctrl);

        if (m_nLatencyStats) {
            if (!m_bStartLatency) {
                m_bStartLatency = true;
            }
            m_Pkt.av_pkt.pts = m_mProfTimer["pkt_latency"]->profTimerBegin();
        }

        m_notified = true;
    }

    DEBUG_LOG("Lock Released");
    m_cv.notify_one();

    return 0;
}

int CIrrVideoDemux::setLatencyStats(int nLatencyStats) {
    std::lock_guard<std::mutex> lock(m_Lock);

    m_nLatencyStats = nLatencyStats;
    if (m_nLatencyStats) {
        if (m_mProfTimer.find("pkt_latency") == m_mProfTimer.end()) {
            m_mProfTimer["pkt_latency"] = new ProfTimer(true);
        }
        m_mProfTimer["pkt_latency"]->setPeriod(nLatencyStats);
        m_mProfTimer["pkt_latency"]->enableProf();

        if (m_mProfTimer.find("pkt_round") == m_mProfTimer.end()) {
            m_mProfTimer["pkt_round"] = new ProfTimer(true);
        }
        m_mProfTimer["pkt_round"]->setPeriod(nLatencyStats);
        m_mProfTimer["pkt_round"]->enableProf();
        m_mProfTimer["pkt_round"]->profTimerBegin();

    }else{
        if (m_bStartLatency) {
            if (m_mProfTimer.find("pkt_latency") != m_mProfTimer.end()) {
                m_mProfTimer["pkt_latency"]->profTimerReset("pkt_latency");
            }
        }

        if (m_mProfTimer.find("pkt_round") != m_mProfTimer.end()) {
            m_mProfTimer["pkt_round"]->profTimerReset("pkt_round");
        }

        m_bStartLatency = false;
    }

    return 0;
}
