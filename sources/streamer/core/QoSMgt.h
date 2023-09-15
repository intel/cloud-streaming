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

#ifndef __QOS_MGT_H__
#define __QOS_MGT_H__

#include "ga-common.h"

/**
* Struct: IRD_CURSOR_INFO
* @brief This structure is used by Titan SDK client to get cursor info for current frame.
*/
typedef struct _QosInfo
{
    unsigned int   frameno;
    unsigned int   framesize;
    unsigned int   bitrate;
    unsigned int   captureFps;
    struct timeval eventime;
    double         capturetime;
    double         encodetime;
    unsigned int   estimated_bw;
} QosInfo;

EXPORT int queue_qos(QosInfo qosInfo);

#endif
