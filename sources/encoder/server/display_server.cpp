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

#include <string>
#include <assert.h>

#include "display_server.h"
#include "display_server_vhal.h"
#include "display_video_renderer.h"
#include "utils/TimeLog.h"

DisplayServer::DisplayServer()
{
    sock_log("Creating DisplayServer\n");
    m_renderer = nullptr;
    m_id=0;
}

DisplayServer::~DisplayServer()
{
}

bool DisplayServer::run()
{
    while (true) {
        if (event_flag != 0){
            SOCK_LOG(("%s:%d : get exit event_flag=%d\n", __func__, __LINE__, event_flag));
            break;
        }
        usleep(16000);
    }
    return true;
}

int DisplayServer::event_flag = 0;

void DisplayServer::signal_handler(int signum) {
    switch (signum){
        case SIGINT://Ctrl+C trigger
            SOCK_LOG(("%s:%d : received SIGINT, set event_flag to 1!\n", __func__, __LINE__));
            event_flag = 1;
            break;
        case SIGTERM://Ctrl+\ trigger
            SOCK_LOG(("%s:%d : received SIGTERM, set event_flag to 1!\n", __func__, __LINE__));
            event_flag = 1;
            break;
        case SIGQUIT://kill command will trigger
            SOCK_LOG(("%s:%d : received SIGQUIT, set event_flag to 1!\n", __func__, __LINE__));
            event_flag = 1;
            break;
    default:
        SOCK_LOG(("%s:%d : received a signal that needn't handle!\n", __func__, __LINE__));
        break;
    }
}

DisplayServer *DisplayServer::Create(const char *socket)
{
    return new DisplayServerVHAL();
}

