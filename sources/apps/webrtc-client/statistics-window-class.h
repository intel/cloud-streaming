// Copyright (C) 2020-2023 Intel Corporation
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

#ifndef GA_STATISTICSWINDOWCLASS_H_
#define GA_STATISTICSWINDOWCLASS_H_

#include <Windows.h>
#include <Windowsx.h>

#include <atomic>
#include <cmath>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "implot.h"

#define WM_GA_CURSOR_VISIBLE (WM_USER + 1)
#define GA_HIDE_CURSOR 0
#define GA_SHOW_CURSOR 1

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define MAXSAMPLES 200

struct StreamingStatistics
{
    std::atomic<bool> updated{ false };

    // frame info
    unsigned int captureFps = 0;

    void CalcStatistics(double newTime, double* realTime, double* avgTime, double* minTime, double* maxTime, int* index, double* timesum, double* timelist)
    {
        *realTime = newTime;
        if (newTime < *minTime || *minTime == 0)
            *minTime = newTime;
        if (newTime > *maxTime || *maxTime == 0)
            *maxTime = newTime;

        *timesum -= timelist[*index];
        *timesum += newTime;
        timelist[*index] = newTime;
        if (++(*index) == MAXSAMPLES)
            *index = 0;

        /* get average */
        if (frametimesum != 0) {
            *avgTime = *timesum / (double)MAXSAMPLES;
            double multiplier = std::pow(10.0, 6);
            *avgTime = std::round(*avgTime * multiplier) / multiplier;
        }
    }

    //Client Render Statistics
    double crenrealtime = 0;
    double crenavgtime = 0;
    double crenmintime = 0;
    double crenmaxtime = 0;
    int crenindex = 0;
    double crentimesum = 0;
    double crentimelist[MAXSAMPLES];

    //Server Render Statistics
    double srenrealtime = 0;
    double srenavgtime = 0;
    double srenmintime = 0;
    double srenmaxtime = 0;
    int srenindex = 0;
    double srentimesum = 0;
    double srentimelist[MAXSAMPLES];

    //Decode Statistics
    double decrealtime = 0;
    double decavgtime = 0;
    double decmintime = 0;
    double decmaxtime = 0;
    int decindex = 0;
    double dectimesum = 0;
    double dectimelist[MAXSAMPLES];

    //Encode Statistics
    double encrealtime = 0;
    double encavgtime = 0;
    double encmintime = 0;
    double encmaxtime = 0;
    int    encindex = 0;
    double enctimesum = 0;
    double enctimelist[MAXSAMPLES];

    //E2E Latency Statistics
    double e2erealtime = 0;
    double e2eavgtime = 0;
    double e2emintime = 0;
    double e2emaxtime = 0;
    int    e2eindex = 0;
    double e2etimesum = 0;
    double e2etimelist[MAXSAMPLES];

    //Frame Size Statistics
    double framesizerealtime = 0;
    double framesizeavgtime = 0;
    double framesizemintime = 0;
    double framesizemaxtime = 0;
    int    framesizeindex = 0;
    double framesizetimesum = 0;
    double framesizetimelist[MAXSAMPLES];

    //Frame Delay Statistics
    double framedelayrealtime = 0;
    double framedelayavgtime = 0;
    double framedelaymintime = 0;
    double framedelaymaxtime = 0;
    int    framedelayindex = 0;
    double framedelaytimesum = 0;
    double framedelaytimelist[MAXSAMPLES];

    //Packet Loss Statistics
    double packetlossrealtime = 0;
    double packetlossavgtime = 0;
    double packetlossmintime = 0;
    double packetlossmaxtime = 0;
    int    packetlossindex = 0;
    double packetlosstimesum = 0;
    double packetlosstimelist[MAXSAMPLES];

    unsigned int frametimeindex = 0;
    unsigned int frametimesamples = 0;
    double frametimesum = 0;
    double frametimelist[MAXSAMPLES];

    uint16_t framewidth = 0;
    uint16_t frameheight = 0;

    void init() {
        memset(frametimelist     , 0, sizeof(frametimelist)     );
        memset(crentimelist      , 0, sizeof(crentimelist)      );
        memset(srentimelist      , 0, sizeof(srentimelist)      );
        memset(dectimelist       , 0, sizeof(dectimelist)       );
        memset(enctimelist       , 0, sizeof(enctimelist)       );
        memset(e2etimelist       , 0, sizeof(e2etimelist)       );
        memset(framesizetimelist , 0, sizeof(framesizetimelist) );
        memset(framedelaytimelist, 0, sizeof(framedelaytimelist));
        memset(packetlosstimelist, 0, sizeof(packetlosstimelist));
    }

    void CalcFPS(double newFrameTime)
    {
        frametimesum -= frametimelist[frametimeindex];
        frametimesum += newFrameTime;
        frametimelist[frametimeindex] = newFrameTime;
        if (++frametimeindex == std::size(frametimelist)) {
            frametimeindex = 0;
        }
        if (frametimesamples < std::size(frametimelist)) {
            frametimesamples++;
        }

        /* return fps */
        if (frametimesum != 0) {
            captureFps = (unsigned int)(frametimesamples / frametimesum);
        }
    }
};

class StatisticsWindowClass {
public:
    HWND hwnd_;

    StatisticsWindowClass(HINSTANCE h_instance, int n_cmd_show);
    void Destroy(void);
    void DrawStatistics();
    void setStreamingStatistics(StreamingStatistics* streamingStatistics) { _streamingStatistics  = streamingStatistics;};
private:
    StreamingStatistics* _streamingStatistics;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    WNDCLASSEX wc_;

};

namespace ImPlot {
    void ShowImplotWindow(StreamingStatistics* streamingStatistics);
}

#endif // GA_STATISTICSWINDOWCLASS_H_
