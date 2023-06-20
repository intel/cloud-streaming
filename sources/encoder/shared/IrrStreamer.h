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

#ifndef IRRSTREAMER_H
#define IRRSTREAMER_H

#include <map>
#include <inttypes.h>
#include <stddef.h>

#include "api/irrv-internal.h"
#include "utils/CTransLog.h"
#include "CCallbackMux.h"
#include "CIrrVideoDemux.h"
#include "CTransCoder.h"
#include "utils/IOStreamWriter.h"
#include "utils/IORuntimeWriter.h"
#include "irrv/irrv_protocol.h"

#define MIN_RESOLUTION_VALUE_H264 32
#define MIN_RESOLUTION_VALUE_HEVC 128
#define MIN_RESOLUTION_VALUE_AV1 128

class IrrStreamer : public CTransLog {
public:
    static IrrStreamer* get();
    static void Register(int id, int w, int h, float framerate);
    static void Unregister();

    IrrStreamer(int id, int w, int h, float framerate);
    IrrStreamer(const IrrStreamer&) = delete;
    IrrStreamer& operator=(const IrrStreamer&) = delete;
    ~IrrStreamer();

    int   start(IrrStreamInfo *param);
    void  stop();
    int   write(irr_surface_t* surface);
    int   generate_packet(irr_surface_t* surface, IrrPacket& pkt);
    int   force_key_frame(int force_key_frame);
    int   set_qp(int qp);
    int   set_bitrate(int bitrate);
    int   set_max_bitrate(int max_bitrate);
    int   set_max_frame_size(int size);
    int   set_rolling_intra_refresh(int type, int cycle_size, int qp_delta);
#ifdef FFMPEG_v42
    int   set_region_of_interest(int roi_num, AVRoI roi_para[]);
#endif
    int   set_min_max_qp(int min_qp, int max_qp);
    int   change_resolution(int width, int height);
    int   change_codec(AVCodecID codec_type);
    int   setLatency(int latency);
    int   getWidth();
    int   getHeight();
    int   getEncoderType();
    int   set_framerate(float framerate);
    int   get_framerate(void);
    int   set_sei(int sei_type, int sei_user_id);
    int   set_gop_size(int size);
    void  set_screen_capture_flag(bool bAllowCapture);
    void  set_screen_capture_interval(int captureInterval);
    void  set_screen_capture_quality(int quality_factor);
    void  set_iostream_writer_params(const char *input_file, const int width, const int height,
                                     const char *output_file, const int output_frame_number);

    int   set_client_feedback(unsigned int delay, unsigned int size);

    void setVASurfaceFlag(bool bVASurfaceID);
    bool getVASurfaceFlag();

    void setQSVSurfaceFlag(bool bQSVSurfaceID);
    bool getQSVSurfaceFlag();

    void  incClientNum();
    void  decClientNum();
    int   getClientNum();
    void  setEncodeFlag(bool bAllowEncode);
    bool  getEncodeFlag();
    void  setTransmitFlag(bool bAllowTransmit);
    bool  getTransmitFlag();
    void  setFisrtStartEncoding(bool bFirstStartEncoding);
    bool  getAuthFlag();
    void  hwframe_ctx_init();
    int   set_hwframe_ctx(AVBufferRef *hw_device_ctx);
    AVBufferRef* createAvBuffer(int size);
    void set_output_prop(CTransCoder *m_pTrans, IrrStreamInfo *param);

    void  set_crop(int client_rect_right, int client_rect_bottom, int fb_rect_right, int fb_rect_bottom, 
                   int crop_top, int crop_bottom, int crop_left, int crop_right, int valid_crop);
    IORuntimeWriter::Ptr getRunTimeWriter() { return m_pRuntimeWriter; }

    void  setSkipFrameFlag(bool bSkipFrame);
    bool  getSkipFrameFlag();

    void set_alpha_channel_mode(bool isAlpha);
    void set_buffer_size(int width, int height);

    int getEncodeNewWidth();
    int getEncodeNewHeight();

    /*
    *  * @Desc change the profile and level of the codec
    *  * @param [in] iProfile, iLevel
    *  * @return 0 on success, minus mean fail or not change at all.
    */
    int change_profile_level(const int iProfile, const int iLevel);

    void setRenderFpsEncFlag(bool bRenderFpsEnc);
    /**
    * get the encode by render fps flag.
    *
    * @return minus mean that the call of the function fail, 1 mean that encode by render fps is trun on, 0 mean turn off.
    */
    int getRenderFpsEncFlag(void);

private:
    CIrrVideoDemux *m_pDemux;
    CTransCoder    *m_pTrans;
    CCallbackMux   *m_pMux;
    IOStreamWriter *m_pWriter;
    IORuntimeWriter::Ptr m_pRuntimeWriter;
    AVBufferPool  *m_pPool = nullptr;
    int            m_nMaxPkts;   ///< Max number of cached frames
    int            m_nCurPkts;
    AVPixelFormat  m_nPixfmt;
    int            m_nWidth, m_nHeight;
    int            m_nCodecId;
    float          m_fFramerate;
    std::mutex     m_Lock;
    bool m_bVASurface;
    bool m_bQSVSurface;
    int            m_nClientNum;
    bool           m_bAllowEncode;
    bool           m_bAllowTransmit;
    int            m_id;
    bool           m_auth;
    AVBufferRef   *m_hw_frames_ctx;
    bool           m_tcaeEnabled;

    /// A blank surface is allocated and used to initialize CIrrVideoDemux::m_Pkt
    /// This is required for the scenario where app flow calls CIrrVideoDemux::readPacket
    /// before CIrrVideoDemux::sendPacket. A valid packet is required for the encoder
    irr_surface_t* m_blankSurface = nullptr;

    int InitBlankFramePacket(IrrPacket& pkt);
    int DeinitBlankFramePacket();

#ifdef FFMPEG_v42
    static AVBufferRef* m_BufAlloc(void *opaque, int size);
#else
    static AVBufferRef* m_BufAlloc(void *opaque, size_t size);
#endif
};

#endif /* IRRSTREAMER_H */
