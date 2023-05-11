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

#ifndef CFFFILTER_H
#define CFFFILTER_H

#include "CFilter.h"
#include "CStreamInfo.h"
#include "utils/CTransLog.h"
extern "C" {
#include <libavfilter/avfilter.h>
//#include <libavfilter/avfiltergraph.h>
#include "libavutil/log.h"
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#define DEFAULT_FILTER_NBTHREADS 1

class CFFFilter : public CFilter, private CTransLog {
public:
    CFFFilter(CStreamInfo *in, CStreamInfo *out, bool bVASurface, bool bQSVSurface, bool isVaapiPlugin, bool isQSVPlugin, const int filter_nbthreads);
    ~CFFFilter();
    int push(AVFrame *frame);
    AVFrame* pop();
    int getNumFrames(void);
    CStreamInfo *getSinkInfo();
    CStreamInfo *getSrcInfo();

    void setVASurfaceFlag(bool bVASurface);
    bool getVASurfaceFlag();
    void setQSVSurfaceFlag(bool bQSVSurface);
    bool getQSVSurfaceFlag();
    int getLastError();
    void setLastError(int iErrorCode);
    void updateDynamicChangedFramerate(int framerate);

private:
    AVFilterGraph *m_pGraph = nullptr;
    bool           m_bInited = false;
    AVFilterContext *m_pSrc = nullptr, *m_pSink = nullptr;
    size_t         m_nFrames = 0;
    CStreamInfo    m_SrcInfo, m_SinkInfo;
    bool m_bVASurface = false;
    bool m_bQSVSurface = false;
    bool m_vaapiPlugin = false;
    bool m_qsvPlugin = false;
    int m_lastError = 0;
private:
    AVFilterContext* alloc_filter(const char *name, const char *par);
};

#endif /* CFFFILTER_H */

