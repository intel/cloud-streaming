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

#ifndef __IRRV_INTERNAL_H__
#define __IRRV_INTERNAL_H__

#include "api/irrv.h"

#ifdef __cplusplus
extern "C" {
#endif

enum output_mux_type {
    IRRV_MUX = 0,      ///<  irrv output mux
    LOCAL_MUX = 1,     ///<  local output mux
    DEFAULT = 2,       ///<  default
};

typedef enum output_mux_type output_mux_type_t;

typedef struct _irr_rate_ctrl_options_info {
    bool need_qp;
    bool need_qfactor;
    bool need_bitrate;
    bool need_maxbitrate;
} irr_rate_ctrl_options_info_t;

extern VADisplay va_dpy;

/**
 * @param encoder_info_t encoder information
 * @desc                check the profile parameters in encoder_info_t.
 */
int irr_check_encode_profile(encoder_info_t *encoder_info);

/**
 * @param encoder_info_t encoder information
 * @desc                check the level parameters in encoder_info_t.
 */
int irr_check_encode_level(encoder_info_t *encoder_info);

/*
 * @param encoder_info_t encoder information
 * @desc                check the rolling intra refresh parameters in encoder_info_t.
 */
int irr_check_rir_options(encoder_info_t *encoder_info);

/*
 * @param encoder_info_t encoder information
 * @desc                check the region of interest parameters in encoder_info_t.
 */
int irr_check_roi_options(encoder_info_t *encoder_info);

/**
*
* @param no
* @desc                        destroy the VADisplay.
*/
void irr_encoder_destroy_display();

/**
*
* @param client_rect_right     client(device end) rect right coordinate
* @param client_rect_bottom    client(device end) rect bottom coordinate
* @param fb_rect_right         fb rect right coordinate
* @param fb_rect_bottom        fb rect bottom coordinate
* @param crop_top              crop offset from top border
* @param crop_bottom           crop offset from bottom border
* @param crop_left             crop offset from left border
* @param crop_right            crop offset from right border
* @param valid_crop            crop valid or not
* @desc                        set encoder crop info.
*/
void irr_encoder_write_crop(int client_rect_right, int client_rect_bottom, int fb_rect_right, int fb_rect_bottom, 
                            int crop_top, int crop_bottom, int crop_left, int crop_right, int valid_crop);

/**
*
* @param width                 width
* @param height                height
* @desc                        change the encoder's resolution
*/
int irr_encoder_change_resolution(int width,int height);

/**
*
* @param isAlpha               alpha channel mode
* @desc                        set the alpha channel mode
*/
void irr_encoder_set_alpha_channel_mode(bool isAlpha);

/**
*
* @param width                 width
* @param height                height
* @desc                        change the encodeing buffer size
*/
void irr_encoder_set_buffer_size(int width,int height);

/**
*
* @desc                        get the vasurface flag.
*/
int irr_encoder_get_VASurfaceFlag();

/**
*

* @desc                        get the qsvsurface flag.
*/
int irr_encoder_get_QSVSurfaceFlag();

/* @desc                        get the frame rate.
*/
int irr_encoder_get_framerate(void);


/*
* @Desc set encode by render fps flag.
* @param bRenderFpsEnc          true mean that flag turn on, false mean turn off.
*/
void irr_encoder_set_encode_renderfps_flag(bool bRenderFpsEnc);

/*
* @Desc get encode by render fps flag.
* @return minus mean call the function fail, 1 mean that flag turn on, 0 mean turn off.
*/
int irr_encoder_get_encode_renderfps_flag(void);

/*
* @Desc set the skip frame flag.
* @param bSkipFrame, true mean that flag turn on, false mean turn off.
*/
void irr_encoder_set_skipframe(bool bSkipFrame);

/*
* @Desc get skip frame flag.
* @return minus mean call the function fail, 1 mean that flag turn on, 0 mean turn off.
*/
int irr_encoder_get_skipframe(void);

struct IrrStreamInfo {
    int pix_format;            ///< fmt
    /* Output-only parameters */
    int gop_size;              ///< Group Of Picture size, default 120
    const char *codec;         ///< Encoder codec, e.x. h264_qsv; may be null
    const char *format;        ///< Mux format, e.x. flv; null as auto
    const char *url;           ///< Output url.
    int low_power;             ///< Enable low-power mode, default not.
    const char *res;           ///< Encoding resolution.
    const char *framerate;     ///< Encoding framerate
    const char *exp_vid_param; ///< Extra encoding/muxer parameters passed to libtrans/FFmpeg
    bool bVASurface;
    rate_control_info_t rc_params;
    int quality;               ///< Encoding quality level
    int max_frame_size;        ///< Encoding max frame size
    irr_ref_info_t ref_info;
    irr_roi_info_t roi_info;
    int slices;                ///< Encoder number of slices, used in parallelized encoding
    int sei;                   ///< Encoding SEI information
    int latency_opt;           ///< Encoding latency optimization, set 1 to enable, 0 to disable
    bool auth;                 ///< Enable Socket authentication
    int renderfps_enc;         ///< Encoding by rendering fps, set 1 to enable, 0 to disable, default is 0.
    int minfps_enc;            ///< min encode fps when renderfps_enc is used.
    const char *profile;       ///< Encoder profile
    const char *level;         ///< Encoder level
    int filter_nbthreads;      ///< filter threads number
    bool low_delay_brc;        ///< enable TCBRC that trictly obey average frame size set by target bitarte
    bool skip_frame;           ///< enable Skip Frame
    const char *plugin;        ///< Encoder plugin option
    bool bQSVSurface;          ///< Is QSV Surface used
    bool tcaeEnabled;          ///< Is TCAE enabled
    const char *tcaeLogPath;   ///< TCAE log file path

    struct CallBackTable {     ///< Callback function tables
        void *opaque;          ///< Used by callback functions
        void *opaque2;         ///< Used by callback functions
        int (*cbOpen) (void* opaque, int w, int h, float frame_rate);
        /* Synchronous write callback*/
        int (*cbWrite) (void* opaque, uint8_t* data, size_t size, unsigned int flags);
        int (*cbWrite2) (void* opaque, uint8_t* data, size_t size, int type);
        void (*cbClose) (void* opaque);
        int (*cbCheckNewConn) (void* opaque);
        int (*cbSendMessage)(void* opaque, int msg, unsigned int value);
    } cb_params;
};

/**
 * @param stream_info
 * @return 0 on success
 */
int irr_stream_start(IrrStreamInfo *stream_info);

void irr_stream_stop();

/*
 * @Desc force key frame
 */
int irr_stream_force_keyframe(int force_key_frame);

/*
 * @Desc set QP
 */
int irr_stream_set_qp(int qp);

/*
 * @Desc set bitrate
 */
int irr_stream_set_bitrate(int bitrate);

/*
 * @Desc set max bitrate
 */
int irr_stream_set_max_bitrate(int max_bitrate);

/*
 * @Desc set framerate
 */
int irr_stream_set_framerate(float framerate);

/*
* @Desc get encode framerate
*/
int irr_stream_get_framerate(void);

/*
 * @Desc set max frame size
 */
int irr_stream_set_max_frame_size(int size);

/*
 * @Desc set rolling intra refresh
 */
int irr_stream_set_rolling_intra_refresh(int type, int cycle_size, int qp_delta);

/*
 * @Desc set region of interest
 */
#ifdef FFMPEG_v42
int irr_stream_set_region_of_interest(int roi_num, AVRoI roi_para[]);
#endif

/*
 * @Desc set min qp and max qp
 */
int irr_stream_set_min_max_qp(int min_qp, int max_qp);

/*
 *  * @Desc change resolution
 *   */
int irr_stream_change_resolution(int width, int height);

/*
*  * @Desc change codec
*   */
int irr_stream_change_codec(AVCodecID codec_type);

/*
 * @Desc latency start/stop/param setting.
 */
int irr_stream_latency(int latency);

/*
* @Desc get stream width.
*/
int irr_stream_get_width();

/*
* @Desc get stream height.
*/
int irr_stream_get_height();

/*
* @Desc get encoder type id.
*/
int irr_stream_get_encoder_type();

enum IRR_RUNTIME_WRITE_MODE {
    IRR_RT_MODE_INPUT,
    IRR_RT_MODE_OUTPUT,
    IRR_RT_MODE_BOTH,
};

void irr_stream_runtime_writer_start(const enum IRR_RUNTIME_WRITE_MODE mode);
void irr_stream_runtime_writer_stop(const enum IRR_RUNTIME_WRITE_MODE mode);
void irr_stream_runtime_writer_start_with_frame_num(const int frame_num);

int irr_get_VASurfaceFlag();

int irr_get_QSVSurfaceFlag();

void irr_stream_decClient();

int irr_stream_getClientNum();

bool irr_stream_getEncodeFlag();

bool irr_stream_getTransmitFlag();

void irr_stream_first_start_encdoding(bool bFirstStartEncoding);

/**
 * @Desc set sei type and user id
 */
int irr_stream_set_sei(int sei_type, int sei_user_id);

/**
 * @Desc set gop size
 */
int irr_stream_set_gop_size(int size);

bool irr_stream_getAuthFlag();

void irr_stream_set_screen_capture_flag(bool bAllowCapture);

void irr_sream_set_screen_capture_interval(int captureInterval);

void irr_stream_set_screen_capture_quality(int quality_factor);

void irr_stream_set_iostream_writer_params(const char *input_file, const int width, const int height,
                                           const char *output_file, const int output_frame_number);

void irr_stream_set_crop(int client_rect_right, int client_rect_bottom, int fb_rect_right, int fb_rect_bottom, 
                         int crop_top, int crop_bottom, int crop_left, int crop_right, int valid_crop);


void irr_stream_set_skipframe(bool bSkipFrame);

/*
* @Desc get skip frame flag.
* @return minus mean call the function fail, 1 mean that flag turn on, 0 mean turn off.
*/
int irr_stream_get_skipframe();

void irr_stream_set_alpha_channel_mode(bool isAlpha);

void irr_stream_set_buffer_size(int width, int height);

int irr_stream_get_encode_new_width();

int irr_stream_get_encode_new_height();

/*
*  * @Desc change the profile and level of the codec
*  * @param [in] iProfile, iLevel
*  * @return 0 on success, minus mean fail or not change at all.
*   */
int irr_stream_change_profile_level(const int iProfile, const int iLevel);

void irr_stream_set_encode_renderfps_flag(bool bRenderFpsEnc);

/*
* @Desc get encode by reder fps flag, minus mean call the function fail, 1 mean that flag turn on, 0 mean turn off.
*/
int irr_stream_get_encode_renderfps_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* __IRRV_INTERNAL_H__ */

