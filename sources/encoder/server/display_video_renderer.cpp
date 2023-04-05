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

#include <va/va.h>

#include "display_video_renderer.h"
#include "utils/TimeLog.h"

#define DRV_MAX_PLANES 4

using namespace std;

static inline uint64_t getUs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ll + ts.tv_nsec / 1000;
}

DisplayVideoRenderer::DisplayVideoRenderer()
:DisplayRenderer()
{
    SOCK_LOG_INIT();

    m_width         = ENCODER_RESOLUTION_WIDTH_DEFAULT;
    m_height        = ENCODER_RESOLUTION_HEIGHT_DEFAULT;
    m_frameIdx      = 0;

    m_blankSurface  = nullptr;
    m_curSurface    = nullptr;
}

DisplayVideoRenderer::~DisplayVideoRenderer()
{
    SOCK_LOG_INIT();

    flushDelayDelRes();

    if (m_blankSurface) {
        m_blankSurface->ref_count = 1;
        irr_encoder_unref_surface(m_blankSurface);
        m_blankSurface = nullptr;
    }
}

bool DisplayVideoRenderer::init(char *name, encoder_info_t *info)
{
    SOCK_LOG_INIT();

    m_currentInfo = *info;

    //encode process lunch
    irr_encoder_start(m_id, info);

    SOCK_LOG(("rendering and streaming with resolution %s\n", info->res));
    if(info->res) {
        sscanf(info->res, "%dx%d", &m_width, &m_height);
        if( m_width  < ENCODER_RESOLUTION_WIDTH_MIN  || m_width  > ENCODER_RESOLUTION_WIDTH_MAX ||
            m_height < ENCODER_RESOLUTION_HEIGHT_MIN || m_height > ENCODER_RESOLUTION_HEIGHT_MAX) {

            m_width  = ENCODER_RESOLUTION_WIDTH_DEFAULT;
            m_height = ENCODER_RESOLUTION_HEIGHT_DEFAULT;
        }
    }

    return true;
}

void DisplayVideoRenderer::deinit()
{
    SOCK_LOG_INIT();

    //Flush out unused surfaces
    retireFrame();

    //encode process stop
    irr_encoder_stop();
}

disp_res_t* DisplayVideoRenderer::createDispRes(vhal::client::cros_gralloc_handle_t handle)
{
    SOCK_LOG(("%s:%d : handle = %p\n", __func__, __LINE__, handle));

    disp_res_t* res = new disp_res_t();
    if(res) {

        vhal::client::buffer_handle_t baseData = &(handle->base);
        res->local_handle= baseData;

        {
            int MaxPayloadSize = (sizeof(*handle) - sizeof(*baseData))/sizeof(uint32_t);
            if (baseData->numInts > MaxPayloadSize) {
                sock_log("%s:%d : total num of ints in data array is wrong !numFds = %d, numInts = %d, MaxPayloadSize = %d\n",
                          __func__, __LINE__, baseData->numFds, baseData->numInts, MaxPayloadSize);
            }
        }

        res->width          = handle->width;
        res->height         = handle->height;
        res->drm_format     = handle->format;
        res->android_format = handle->droid_format;
        res->seq_no         = 0;

        for (int i = 0; i < DRV_MAX_PLANES; i++) {
            res->prime_fds[i]   = handle->fds[i];
            res->strides[i]     = handle->strides[i];
            res->offsets[i]     = handle->offsets[i];
        }


        SOCK_LOG(("%s:%d : create disp res for : \n", __func__, __LINE__));
        SOCK_LOG(("%s:%d : width = %d, height=%d, drm_format = 0x%x, android_format=%d, seq_no = %u\n", __func__, __LINE__,
                res->width, res->height, res->drm_format, res->android_format, res->seq_no));

        if (baseData->numFds <= DRV_MAX_PLANES) {
            for (int i = 0; i < baseData->numFds; i++) {
                SOCK_LOG(("%s:%d : plane [%d] : prime_fd = %d, stride =%d, offset = %d\n", __FUNCTION__, __LINE__,
                    i, res->prime_fds[i], res->strides[i], res->offsets[i]));
            }
        }
        else {
            sock_log("%s:%d : handle->base.numFds is wrong! handle->base.numFds = %d\n",
                __func__, __LINE__, baseData->numFds);
        }

        irr_surface_info_t info;
        memset(&info, 0, sizeof(info));

        info.type       = FD;
        info.format     = res->drm_format;
        info.width      = res->width;
        info.height     = res->height;

        for (int i = 0; i < MAX_PLANE_NUM; i++) {
            info.stride[i]   = res->strides[i];
            info.offset[i]   = res->offsets[i];
            info.fd[i]      = res->prime_fds[i];
        }
        info.data_size  = 0;
        info.pdata      = NULL;

        res->surface = irr_encoder_create_surface(&info);
        if (res->surface) {
            SOCK_LOG(("%s : %d: irr_encoder_create_surface succeed, res=%p, prime fd=%d, vaSurfaceID=%d, ref_count=%d\n", __func__, __LINE__,
                res, res->surface->info.fd, res->surface->vaSurfaceID, res->surface->ref_count));
        }
        else {
            sock_log("%s : %d : irr_encoder_create_surface failed!\n", __func__, __LINE__);
            delete res;
            res = NULL;
        }
    }
    return res;
}

void DisplayVideoRenderer::destroyDispRes(disp_res_t* disp_res)
{
    SOCK_LOG(("%s:%d : disp_res = %p\n", __func__, __LINE__, disp_res));
    if(disp_res) {
        m_deletedReses.push_back(make_pair(m_frameIdx, disp_res));
    }
}

void DisplayVideoRenderer::drawDispRes(disp_res_t* disp_res, int client_id, int client_count, std::unique_ptr<vhal::client::display_control_t> ctrl)
{
    SOCK_LOG(("%s:%d : disp_res = %p, client_id = %d, client_count = %d\n", __func__, __LINE__,
            disp_res, client_id, client_count));

    irr_surface_t* surface = nullptr;

    if(disp_res) {

        SOCK_LOG(("%s:%d : width = %d, height=%d, drm_format = 0x%x, android_format=%d, seq_no = %u\n", __func__, __LINE__,
                disp_res->width, disp_res->height, disp_res->drm_format, disp_res->android_format, disp_res->seq_no));

        surface = disp_res->surface;
    }

    if(!surface) {
        // Use blank surface if disp_res is null, or disp_res->surface is null.
        if(!m_blankSurface) {
            irr_surface_info_t info;
            memset(&info, 0, sizeof(info));

            info.type       = FD;
            info.width      = m_width;
            info.height     = m_height;
            for (int i = 0; i < MAX_PLANE_NUM; i++) {
                info.fd[i]  = -1;
            }
            m_blankSurface = irr_encoder_create_blank_surface(&info);
            if(!m_blankSurface) {
                SOCK_LOG(("%s : %d : irr_encoder_create_blank_surface failed\n", __func__, __LINE__));
            }
        }
        surface = m_blankSurface;
    }

    if(surface) {
        int ret = 0;
        ATRACE_NAME("irr_encoder_write");
        TimeLog timelog("IRRB_irr_encoder_write");

        surface->display_ctrl = std::move(ctrl);

        ret = irr_encoder_write(surface);
        if(ret != 0) {
            SOCK_LOG(("%s:%d : irr_encoder_write(%p) failed!\n", __func__, __LINE__, surface));
        }
    }

    if(m_curSurface != surface) {
        // unref old surface
        if(m_curSurface) {
            SOCK_LOG(("%s : %d : before call irr_encoder_unref_surface, m_curSurface=%p, prime fd=%d, vaSurfaceID=%d, ref_count=%d\n",
                __func__, __LINE__, m_curSurface, m_curSurface->info.fd, m_curSurface->vaSurfaceID, m_curSurface->ref_count));
            irr_encoder_unref_surface(m_curSurface);
        }

        // ref new surface
        if(surface) {
            irr_encoder_ref_surface(surface);
            SOCK_LOG(("%s : %d : after call irr_encoder_ref_surface, surface=%p, prime fd=%d, vaSurfaceID=%d, ref_count=%d\n",
                __func__, __LINE__, surface, surface->info.fd, surface->vaSurfaceID, surface->ref_count));
        }

        m_curSurface = surface;
    }
}

void DisplayVideoRenderer::drawBlankRes(int client_id, int client_count)
{

    SOCK_LOG(("%s:%d : client_id = %d, client_count = %d\n", __func__, __LINE__, client_id, client_count));
    drawDispRes(nullptr, client_id, client_count, nullptr);
}

void DisplayVideoRenderer::beginFrame()
{
    //    SOCK_LOG_INIT();
    m_frameIdx++;
}

void DisplayVideoRenderer::endFrame()
{
    if(m_fpsStats) {
        uint64_t currTimeInMs = getUs()/1000;
        m_statsNumFrames++;
        if (currTimeInMs - m_statsStartTimeInMs >= 1000) {
            float dtInSec = (float) (currTimeInMs - m_statsStartTimeInMs) / 1000.0f;
            float fps = (float)m_statsNumFrames / dtInSec;
            m_statsStartTimeInMs = currTimeInMs;
            m_statsNumFrames = 0;

            publishStatusToResourceMonitor((uint32_t)m_id, &fps);
        }
    }
}

void DisplayVideoRenderer::retireFrame()
{
    //    SOCK_LOG_INIT();

    list<pair<uint64_t, disp_res_t*>>::iterator iter;
    for(iter = m_deletedReses.begin(); iter!= m_deletedReses.end();) {
        uint64_t  resFrameIdx = iter->first;
        disp_res_t* res = iter->second;
        uint64_t  curFrameIdx = m_frameIdx;
        uint64_t  frameAge = 0;

        if(resFrameIdx <= curFrameIdx) {
            frameAge = curFrameIdx - resFrameIdx;
        }
        else {
            iter->first = 0;
            resFrameIdx = iter->first;
            frameAge = curFrameIdx - resFrameIdx;
        }

        if(frameAge > 30) {
            if(res) {
                SOCK_LOG(("%s : %d : delete res = %p, m_deletedReses.size=%d, prime fd=%d, vaSurfaceID=%d, ref_count=%d, curFrameIdx=%d, resFrameIdx=%d, frameAge=%d\n",
                    __func__, __LINE__, res, m_deletedReses.size(), res->surface->info.fd, res->surface->vaSurfaceID, res->surface->ref_count, curFrameIdx, resFrameIdx, frameAge));
                if(res->surface) {
                    VASurfaceID currEncodeID = VA_INVALID_SURFACE;
                    if (m_curSurface) {
                        currEncodeID = m_curSurface->vaSurfaceID;
                    }
                    if (res->surface->vaSurfaceID != currEncodeID) {
                        SOCK_LOG(("%s : %d : before call irr_encoder_unref_surface, res=%p, prime fd=%d, vaSurfaceID=%d, ref_count=%d\n", __func__, __LINE__,
                            res, res->surface->info.fd, res->surface->vaSurfaceID, res->surface->ref_count));
                        res->surface->ref_count = 1;
                        irr_encoder_unref_surface(res->surface);
                        res->surface = NULL;
                    }
                }
                delete res;
                iter->second = NULL;
            }

            m_deletedReses.erase(iter++);
        }
        else {
            iter++;
        }
    }
}

void DisplayVideoRenderer::flushDelayDelRes() {
    // Flush delay free resource list
    list<pair<uint64_t, disp_res_t*>>::iterator iter;
    bool bDelay = false;
    for (iter = m_deletedReses.begin(); iter != m_deletedReses.end();) {
        disp_res_t* res = iter->second;
        if (res) {
            if (res->surface) {
                if (!bDelay) {
                    //sleep 30 frames based on 30fps, 30x33ms = 30x33x1000us for delay destroy the surfaces.
                    usleep(990000);
                    bDelay = true;
                }
                SOCK_LOG(("%s : %d : before call irr_encoder_unref_surface, m_deletedReses.size=%d, res=%p, prime fd=%d, vaSurfaceID=%d, ref_count=%d\n",
                    __func__, __LINE__, m_deletedReses.size(), res, res->surface->info.fd, res->surface->vaSurfaceID, res->surface->ref_count));
                res->surface->ref_count = 1;
                irr_encoder_unref_surface(res->surface);
                res->surface = NULL;
            }
            delete res;
            iter->second = NULL;
        }

        m_deletedReses.erase(iter++);
    }
}

#if BUILD_FOR_HOST
#define ICRM_FIFO_PATH   "/tmp/icrm-fifo"
#else
#define ICRM_FIFO_PATH   "/ipc/icrm-fifo"
#endif
/*
 * @id: instance id
 */
int DisplayVideoRenderer::publishStatusToResourceMonitor(uint32_t id, void * status) {
    int fd, write_bytes = 0;
    unsigned char write_buffer[128];
    char fifo_path[50];
    char *msg;
    const char *dev_dri;
    int data_len, gpuid, len_to_write = 0;
    const int max_msg_len = sizeof(write_buffer) - 4;
    //Currently, we only need to report fps
    float fps = *(float *)status;
    /* get the gpu id we are on */
    dev_dri = getenv("VAAPI_DEVICE");
    if (!dev_dri)
        dev_dri = "/dev/dri/renderD128";
    sscanf(dev_dri, "/dev/dri/renderD%d", &gpuid);
    if (gpuid > 0 && gpuid <= 256)
    {
        gpuid -= 128;
    }
    else
    {
        return 0;
    }
    snprintf(fifo_path, sizeof(fifo_path), ICRM_FIFO_PATH "-gpu%02d", gpuid);
    fifo_path[sizeof(fifo_path)-1] = '\0';
    //We are fifo producer
    if((fd = open(fifo_path, O_WRONLY | O_NONBLOCK)) <= 0) {
        /*
         * This can be a result that no ICR monitor instance is running
         * or it is merely not interested in the local instance info
         * (so that it's not waiting data coming out of the fifo)
         */
        return fd;
    }
    msg = (char *)write_buffer + 4;
    /*
     * Note: We need to make sure data len will not exeed 254,
     *      Otherwise the package len may conflict with
     *      the sync(start of msg) flag
     *
     * 0xff | id | len | msg
     * e.g.
     * 0xff | 1  |  11 | "gfps=30.00"
     */
    //start of msg
    write_buffer[0] = 0xff;
    /*
     * id to be transmitted is truncated to 16 bit with MSB followed by LSB
     */
    write_buffer[1] = (unsigned char)((id >> 8) & 0xfful);
    write_buffer[2] = (unsigned char)(id & 0xfful);
    //snprintf will leave at least one byte for string ending character
    snprintf(msg, max_msg_len, "gfps=%.2f", fps);
    data_len = strlen(msg) + 1;
    if(data_len + 1 >= 0xff) {
        printf("IRR resource monitor frontend: msg lenth too long!!!\n");
        fflush(stdout);
        close(fd);
        return 0;
    }
    write_buffer[3] = strlen(msg) + 1; //including '\0'
    len_to_write = strlen(msg) + 5;
#if 0
    for(int i = 0; i < len_to_write; i++){
        printf("%02x ",(unsigned int)write_buffer[i]);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
#endif
    write_bytes = write(fd, write_buffer, len_to_write);

    close(fd);

    return write_bytes;

}

void DisplayVideoRenderer::ChangeResolution(int width, int height)
{
    m_width = width;
    m_height = height;
    irr_encoder_stop();

    string curRes = to_string(width) + "x" + to_string(height);
    m_currentInfo.res = curRes.c_str();
    m_currentInfo.encodeType = VASURFACE_ID;
    irr_encoder_start(m_id, &m_currentInfo);
}
