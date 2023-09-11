// Copyright (C) 2022 Intel Corporation
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

/**
* @file   ga-param-shared-structure.h
*
* @brief  Declaration of parameter memory shared classes
*
*/
#include <stdlib.h>
#include "ga-common-kmd-safe.h"
#define MAX_ARGV_LEN    2048    // Maximum game's argument length
#define MAX_ID_LEN      32      // Maximum peer/session id token length
/**
 * @brief param_shared_s data structure which defines parameters to share.
 */
  struct param_shared_s
  {
    char config_pathname[_MAX_PATH] = {}; /**< @brief Configure file pathname */
    char ga_root_path[_MAX_PATH] = {};    /**< @brief GA root path */
    char game_dir[_MAX_PATH] = {};        /**< @brief Game path */
    char game_exe[_MAX_PATH] = {};        /**< @brief Game executable */
    char game_argv[MAX_ARGV_LEN] = {};    /**< @brief Game arguments */
    char hook_type[8] = {};               /**< @brief render target hook type */
    char codec_format[8] = {};            /**< @brief Codec format. "avc", "hevc", etc. */
    char server_peer_id[MAX_ID_LEN] = {}; /**< @brief server peer id */
    char client_peer_id[MAX_ID_LEN] = {}; /**< @brief client peer id */
    char logfile[_MAX_PATH] = {};         /**< @brief Logfile name */
    char video_bitrate[16] = {};          /**< @brief Video bitrate */
    Severity loglevel = Severity::INFO;   /**< @brief GA log level severity */
    LUID luid = {};                       /**< @brief Display adapter's LUID (Locally Unique IDentification) */
    bool enable_tcae = true;              /**< @brief TCAE enable flag */
    bool enable_present = false;          /**< @brief Presentation enable flag */
    int width = 0;                        /**< @brief Resolution width */
    int height = 0;                       /**< @brief Resolution height */
    int encode_width = 0;                 /**< @brief Encode width */
    int encode_height = 0 ;               /**< @brief Encode height */
    bool enable_ltr = false;              /**< @brief LTR enable flag */
    char ltr_interval[8] = {};            /**< @brief Distance between current and reference frames */
  };
