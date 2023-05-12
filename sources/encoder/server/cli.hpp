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

#ifndef _CLI_HPP_
#define _CLI_HPP_

#include "api/irrv.h"

#ifdef __cplusplus
extern "C" {
#endif

void icr_print_usage(const char *arg0);

void icr_parse_args(int argc, char *argv[], encoder_info_t &info);

void show_env();

void clear_env();

void show_encoder_info(encoder_info_t *info);

void rir_type_sanity_check(encoder_info_t *info);

int bitrate_ctrl_sanity_check(encoder_info_t *info);

#ifndef BUILD_FOR_HOST
void encoder_properties_sanity_check(encoder_info_t *info);
#endif

#ifdef __cplusplus
}
#endif

#ifndef BUILD_FOR_HOST
#include <sys/system_properties.h>
#include <memory.h>

#define UNSET_PROP         0xFFFE
#define VAAPI_RENDER_BASE  128
#define PARA_MAX           100

struct int_prop {
    const char  *name;
    int         *pvalue;
    int          default_value;
};
struct char_prop {
    const char  *name;
    const char  **ppvalue;
    const char  *default_value;
};

//on or off
#define ICR_ENCODER_INSIDE_ENABLE             "ro.boot.icr.internal"
#define ICR_ENCODER_RNODE                     "ro.acg.rnode"
#define ICR_ENCODER_WIDTH                     "ro.config.media_width"
#define ICR_ENCODER_HEIGHT                    "ro.config.media_height"
#define ICR_ENCODER_RC_MODE                   "ro.config.media_rc_mode"
#define ICR_ENCODER_FPS                       "ro.config.media_fps"
#define ICR_ENCODER_ENCODE_UNLIMIT_ENABLE     "ro.config.encode_unlimit"
#define ICR_ENCODER_CONTAINER_ID              "ro.boot.container.id"
#define ICR_ENCODER_STREAMING                 "sys.icr.media_streaming"
#define ICR_ENCODER_URL                       "sys.icr.media_url"
#define ICR_ENCODER_CODEC                     "sys.icr.media_codec"
#define ICR_ENCODER_BIT_RATE                  "sys.icr.media_bit_rate"
#define ICR_ENCODER_LOW_POWER                 "sys.icr.media_low_power"
#define ICR_ENCODER_QP                        "sys.icr.media_qp"
#define ICR_ENCODER_QFACTOR                   "sys.icr.media_qfactor"
#define ICR_ENCODER_MAXRATE                   "sys.icr.media_maxrate"
#define ICR_ENCODER_MAX_FRAME_SIZE            "sys.icr.media_max_frame_size"
#define ICR_ENCODER_RIR_TYPE                  "sys.icr.media_rir_type"
#define ICR_ENCODER_RIR_CYCLE_SIZE            "sys.icr.media_rir_cycle_size"
#define ICR_ENCODER_RIR_DELTA_QP              "sys.icr.media_rir_delta_qp"
#define ICR_ENCODER_SLICES                    "sys.icr.media_slices"
#define ICR_ENCODER_QMAX_I                    "sys.icr.media_qmaxI"
#define ICR_ENCODER_QMIN_I                    "sys.icr.media_qminI"
#define ICR_ENCODER_QMAX_P                    "sys.icr.media_qmaxP"
#define ICR_ENCODER_QMIN_P                    "sys.icr.media_qminP"
#define ICR_ENCODER_QUALITY                   "sys.icr.media_quality"
#define ICR_ENCODER_BUF_SIZE                  "sys.icr.media_buffer_size"
#define ICR_ENCODER_SEI                       "sys.icr.media_sei"
#define ICR_ENCODER_FINPUT                    "sys.icr.media_finput"
#define ICR_ENCODER_VFRAME                    "sys.icr.media_vframe"
#define ICR_ENCODER_FOUTPUT                   "sys.icr.media_foutput"
#define ICR_ENCODER_LOG_LEVEL                 "sys.icr.media_log_level"
#define ICR_ENCODER_LATENCY_OPTION            "sys.icr.media_latency_opt"
#define ICR_ENCODER_GOP_SIZE                  "sys.icr.media_gop_size"
#define ICR_ENCODER_AUTH                      "sys.icr.media_auth"
#define ICR_ENCODER_AUXILIARY_SERVER_ENABLE   "sys.icr.media_aux_server"
#define ICR_ENCODER_RENDERFPS_ENC             "sys.icr.media_renderfps_enc"
#define ICR_ENCODER_MINFPS_ENC                "sys.icr.media_minfps_enc"
#define ICR_ENCODER_ROI_ENABLE                "sys.icr.media_roi_enable"
#define ICR_ENCODER_ROI_X                     "sys.icr.media_roi_x"
#define ICR_ENCODER_ROI_Y                     "sys.icr.media_roi_y"
#define ICR_ENCODER_ROI_WIDTH                 "sys.icr.media_roi_width"
#define ICR_ENCODER_ROI_HEIGHT                "sys.icr.media_roi_height"
#define ICR_ENCODER_ROI_VALUE                 "sys.icr.media_roi_value"
#define ICR_ENCODER_PROFILE                   "sys.icr.media_profile"
#define ICR_ENCODER_LEVEL                     "sys.icr.media_level"
#define ICR_ENCODER_FILTER_THREADS            "sys.icr.media_filter_threads"
#define ICR_ENCODER_CLIENT_WIDTH              "sys.icr.media_client_width"
#define ICR_ENCODER_CLIENT_HEIGHT             "sys.icr.media_client_height"
#define ICR_ENCODER_LOW_DELAY_BRC             "sys.icr.media_low_delay_brc"
#define ICR_ENCODER_SKIP_FRAME                "sys.icr.media_skip_frame"
#define ICR_ENCODER_VIDEO_ALPHA               "sys.icr.enable_video_alpha"
#define ICR_ENCODER_CROP_TOP                  "sys.icr.crop.top"
#define ICR_ENCODER_CROP_BOTTOM               "sys.icr.crop.bottom"
#define ICR_ENCODER_CROP_LEFT                 "sys.icr.crop.left"
#define ICR_ENCODER_CROP_RIGHT                "sys.icr.crop.right"

#endif // BUILD_FOR_HOST

#endif

