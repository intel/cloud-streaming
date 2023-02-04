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

#ifndef __IRRV_H__
#define __IRRV_H__

#include <stdint.h>
#include <memory>
#include <libvhal/display-protocol.h>
#include <va/va.h>

#ifdef __cplusplus
extern "C" {
#endif

// As a pure C library, ffmpeg includes must be kept under extern "C"
// when used from C++ code.
#include <libavcodec/codec_id.h>

#define RESOLUTION_WIDTH_DEFAULT   576
#define RESOLUTION_WIDTH_MIN       32
#define RESOLUTION_WIDTH_MAX       4096
#define RESOLUTION_HEIGHT_DEFAULT  960
#define RESOLUTION_HEIGHT_MIN      32
#define RESOLUTION_HEIGHT_MAX      4096

#define ENCODER_RESOLUTION_WIDTH_DEFAULT   RESOLUTION_WIDTH_DEFAULT
#define ENCODER_RESOLUTION_WIDTH_MIN       RESOLUTION_WIDTH_MIN
#define ENCODER_RESOLUTION_WIDTH_MAX       RESOLUTION_WIDTH_MAX
#define ENCODER_RESOLUTION_HEIGHT_DEFAULT  RESOLUTION_HEIGHT_DEFAULT
#define ENCODER_RESOLUTION_HEIGHT_MIN      RESOLUTION_HEIGHT_MIN
#define ENCODER_RESOLUTION_HEIGHT_MAX      RESOLUTION_HEIGHT_MAX

#define MAX_PLANE_NUM 4

/**
 * Type of surface, for exmaple a surface which have a prime id,
 * or user allocate buffers(not DRM_PRIME or KERNEL_PRIME), user can use
 * them for create a VASurface.
 */
enum surface_type {
    BUFFER = 0,      ///<  external buffer
    FD = 1,          ///<  the prime id
    INTERNAL = 2,    ///<  */
    EXTERNAL = 3,    ///<  */
};

typedef enum surface_type surface_type_t;

enum encode_type {
    DATA_BUFFER = 0,      ///<  data buffer
    VASURFACE_ID = 1,     ///<  va surface id
    QSVSURFACE_ID = 2,    ///<  QSV surface id
};

typedef enum encode_type encode_type_t;

typedef struct _rate_control_info_t {
    const char *bitrate;       ///< Encoder bitrate, default 1M
    const char *qfactor;       ///< Encoder global quality.
    const char *qp;            ///< Encoder constant QP for CQP mode.
    const char *maxrate;       ///< Encoder max bitrate.
    const char *ratectrl;      ///< Encoder rate control mode.
    const char *bufsize;       ///< Encoding rate control buffer size (in bits)

    int qmaxI;                 ///< Encoding Maximum video quantizer scale for I frame.
    int qminI;                 ///< Encoding Minimum video quantizer scale for I frame.
    int qmaxP;                 ///< Encoding Maximum video quantizer scale for P frame.
    int qminP;                 ///< Encoding Maximum video quantizer scale for P frame.
} rate_control_info_t;

typedef struct _irr_ref_info {
    const char *int_ref_type;  ///< Encoder intra refresh type.
    int int_ref_cycle_size;    ///< Number of frames in the intra refresh cycle.
    int int_ref_qp_delta;      ///< QP difference for the refresh MBs.
} irr_ref_info_t;

typedef struct _irr_roi_info {
    bool     roi_enabled; ///< Enable region of interest
    int16_t  x;           ///< x position of ROI region.
    int16_t  y;           ///< y position of ROI region.
    uint16_t width;       ///< width of ROI region.
    uint16_t height;      ///< height of ROI region.
    int8_t   roi_value;   ///< roi_value specifies ROI delta QP or ROI priority.
} irr_roi_info_t;

typedef  struct _irr_surface_info {
    int type;                   ///< surface type : buffer, fd, internal, external, ...
    int format;                 ///< fmt , currently the format get from the vhal graph is RGBA and RGB565
    int width;
    int height;

    int stride[MAX_PLANE_NUM];
    int offset[MAX_PLANE_NUM];
    int fd[MAX_PLANE_NUM];                     ///< This is prime id, for example, it maybe got from vhal via sockets.
    int data_size;

    unsigned char*  pdata;                ///< This is buff for store the data.
    unsigned int    reserved[6];
} irr_surface_info_t;

typedef struct _irr_surface_t {
    irr_surface_info_t info;

    int         ref_count;
    VASurfaceID vaSurfaceID;
    int         encode_type;

    int         flip_image;

    void*       mfxSurf;

    std::unique_ptr<vhal::client::display_control_t> display_ctrl;

    unsigned int reserved[5];
} irr_surface_t;

typedef struct _irr_encoder_info {
    int nPixfmt;               ///< fmt
    int gop_size;              ///< Group Of Picture size, default 120
    const char *codec;         ///< Encoder codec, e.x. h264_qsv; may be null
    const char *format;        ///< Mux format, e.x. flv; null as auto
    const char *url;           ///< Output url.
    int low_power;             ///< Enable low-power mode, default not.
    const char *res;           ///< Encoding resolution.
    const char *framerate;     ///< Encoding framerate
    const char *exp_vid_param; ///< Extra encoding/muxer parameters passed to libtrans/FFmpeg
    bool streaming;            ///< streaming true/false
    encode_type_t encodeType;
    int encoderInstanceID;     ///< The encoder instance id, start from 0.
    rate_control_info_t rate_contrl_param;
    int quality;               ///< Encoding quality level
    int max_frame_size;        ///< Encoding max frame size.
    irr_ref_info_t ref_info;
    irr_roi_info_t roi_info;
    int slices;                ///< Encoder number of slices, used in parallelized encoding
    int sei;                   ///< Encoding SEI information
    const char *finput;        ///< Local input file in file dump mode
    int vframe;                ///< Frame number of the input file
    const char *foutput;       ///< Local input file in file dump mode
    const char *loglevel;      ///< Log level to enable icr encoder logs by level
    int latency_opt;           ///< Encoding latency optimization, set 1 to enable, 0 to disable
    bool auth;                 ///< Enable socket authentication
    int renderfps_enc;         ///< Encoding by rendering fps, set 1 to enable, 0 to disable, default is 0.
    int minfps_enc;            ///< Min encode fps when renderfps_enc mode is on.
    const char *profile;       ///< Encoding profile
    const char *level;         ///< Encoding profile level
    int filter_nbthreads;      ///< filter threads number
    bool low_delay_brc;        ///< enable TCBRC that trictly obey average frame size set by target bitarte
    bool skip_frame;           ///< enable Skip Frame
    const char *hwc_sock;      ///< user defined socket name for hwc communication
    const char *plugin;        ///< indicate which plugin is used or not, default vaapi-plugin is used
    bool tcaeEnabled;          ///< indicate whether tcae is enabled or not
    const char * tcaeLogPath;  ///< indicate path to generate tcae dumps. If empty not enabled.
    int user_id;               ///< indicate the user id in mulit-user scenario
} encoder_info_t;

/**
 * @param encoder_info_t encoder information
 * @desc                check the parameters in encoder_info_t.
 */
int irr_check_options(encoder_info_t *encoder_info);

/*
 * @param encoder_info_t encoder information
 * @desc                check the rate ctrl parameters in encoder_info_t.
 */
int irr_check_rate_ctrl_options (encoder_info_t *encoder_info);

/**
 * @param encoder_info_t   encoder information
 * @desc                 Initilize encoder and start the encode process, the parameters are passed into by encode service process
 * following is the parameter value example
 * streaming = 1, res = 0x870e00 "720x1280", b = 0x8723f0 "2M", url = 0x870fa0 "irrv:264",
 *   fr = 0x8710f0 "30", codec = 0x0, lowpower = 1
 */
int irr_encoder_start(int id, encoder_info_t *encoder_info);

/**
 *
 * @desc                 Close encoder and it will clear all the related instances and close the irr sockets and other resources.
 */
void irr_encoder_stop();

/**
 *
 * @param AVCodecID             codec_type
 * @desc                        change the encoder's codec type
 */
int irr_encoder_change_codec(AVCodecID codec_type);

/**
 *
 * @param irr_surface_info_t*   surface information
 * @desc                        create the irr_surface according to the surface info
 */
irr_surface_t* irr_encoder_create_surface(irr_surface_info_t* surface_info);
irr_surface_t* irr_encoder_create_blank_surface(irr_surface_info_t* surface_info);

/**
 *
 * @param irr_surface_t*        surface
 * @desc                        add the reference count of VASurface
 */
void irr_encoder_ref_surface(irr_surface_t* surface);

/**
 *
 * @param irr_surface_t*       surface
 * @desc                       decrease the reference count of VASurface, if zero reference, destroy the vasurface.
 */
void irr_encoder_unref_surface(irr_surface_t* surface);

/**
 *
 * @param irr_surface_t*        surface
 * @desc                        push the surface to encoding list.
 */
int irr_encoder_write(irr_surface_t* surface);

void irr_stream_incClient();

/*
 * @Desc set delay and size from client feedback
 */
int irr_stream_set_client_feedback(uint32_t delay, uint32_t size);

void irr_stream_setEncodeFlag(bool bAllowEncode);

void irr_stream_setTransmitFlag(bool bAllowTransmit);

/*
 * @Desc force key frame
 */
int irr_stream_force_keyframe(int force_key_frame);

#ifdef __cplusplus
}
#endif

#endif /* __IRRV_H__ */

