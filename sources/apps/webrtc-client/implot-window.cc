// MIT License

// Copyright (c) 2022 Evan Pezent
// Copyright (c) 2023 Intel Corporation

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// We define this so that the demo does not accidentally use deprecated API
#ifndef IMPLOT_DISABLE_OBSOLETE_FUNCTIONS
#define IMPLOT_DISABLE_OBSOLETE_FUNCTIONS
#endif

#include "implot.h"
#include "statistics-window-class.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <tuple>
#include <chrono>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

#ifdef _MSC_VER
#define sprintf sprintf_s
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define CHECKBOX_FLAG(flags, flag) ImGui::CheckboxFlags(#flag, (unsigned int*)&flags, flag)

// Encapsulates examples for customizing ImPlot.
namespace MyImPlot {

// Example for Custom Data and Getters section.
struct Vector2f {
    Vector2f(float _x, float _y) { x = _x; y = _y; }
    float x, y;
};

// Example for Custom Data and Getters section.
struct WaveData {
    double X, Amp, Freq, Offset;
    WaveData(double x, double amp, double freq, double offset) { X = x; Amp = amp; Freq = freq; Offset = offset; }
};
ImPlotPoint SineWave(int idx, void* wave_data);
ImPlotPoint SawWave(int idx, void* wave_data);
ImPlotPoint Spiral(int idx, void* wave_data);

// Example for Tables section.
void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size);

// Example for Custom Plotters and Tooltips section.
void PlotCandlestick(const char* label_id, const double* xs, const double* opens, const double* closes, const double* lows, const double* highs, int count, bool tooltip = true, float width_percent = 0.25f, ImVec4 bullCol = ImVec4(0,1,0,1), ImVec4 bearCol = ImVec4(1,0,0,1));

// Example for Custom Styles section.
void StyleSeaborn();

} // namespace MyImPlot

namespace ImPlot {

template <typename T>
inline T RandomRange(T min, T max) {
    T scale = rand() / (T) RAND_MAX;
    return min + scale * ( max - min );
}

ImVec4 RandomColor() {
    ImVec4 col;
    col.x = RandomRange(0.0f,1.0f);
    col.y = RandomRange(0.0f,1.0f);
    col.z = RandomRange(0.0f,1.0f);
    col.w = 1.0f;
    return col;
}

double RandomGauss() {
    static double V1, V2, S;
    static int phase = 0;
    double X;
    if(phase == 0) {
        do {
            double U1 = (double)rand() / RAND_MAX;
            double U2 = (double)rand() / RAND_MAX;
            V1 = 2 * U1 - 1;
            V2 = 2 * U2 - 1;
            S = V1 * V1 + V2 * V2;
            } while(S >= 1 || S == 0);

        X = V1 * sqrt(-2 * log(S) / S);
    } else
        X = V2 * sqrt(-2 * log(S) / S);
    phase = 1 - phase;
    return X;
}

template <int N>
struct NormalDistribution {
    NormalDistribution(double mean, double sd) {
        for (int i = 0; i < N; ++i)
            Data[i] = RandomGauss()*sd + mean;
    }
    double Data[N];
};

// utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 2000) {
        MaxSize = max_size;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    std::tuple<int, int> GetMinMaxY(float x_start, float x_end) {
        int i = 0;
        int ymax = 0;
        int ymin = 9000;

        for (i = 0; i < Data.size(); i++) {
            if (Data[i].x >= x_start && Data[i].x <= x_end){
                if (ymax < (int)Data[i].y) {
                    ymax = (int)Data[i].y;
                }
                if (ymin > (int)Data[i].y) {
                    ymin = (int)Data[i].y;
                }
            }
        }
        return std::tuple<int, int>{ymin, ymax};
    }

    void AddPoint(float x, float y) {
        int i;
        if (y > 2000)
            return;

        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x,y));
        else {
            Data[Offset] = ImVec2(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};

//-----------------------------------------------------------------------------
// [SECTION] Demo Functions
//-----------------------------------------------------------------------------

void Demo_Help() {
    ImGui::Text("ABOUT THIS DEMO:");
    ImGui::BulletText("Sections below are demonstrating many aspects of the library.");
    ImGui::BulletText("The \"Tools\" menu above gives access to: Style Editors (ImPlot/ImGui)\n"
                        "and Metrics (general purpose Dear ImGui debugging tool).");
    ImGui::Separator();
    ImGui::Text("PROGRAMMER GUIDE:");
    ImGui::BulletText("See the ShowDemoWindow() code in implot_demo.cpp. <- you are here!");
    ImGui::BulletText("If you see visual artifacts, do one of the following:");
    ImGui::Indent();
    ImGui::BulletText("Handle ImGuiBackendFlags_RendererHasVtxOffset for 16-bit indices in your backend.");
    ImGui::BulletText("Or, enable 32-bit indices in imconfig.h.");
    ImGui::BulletText("Your current configuration is:");
    ImGui::Indent();
    ImGui::BulletText("ImDrawIdx: %d-bit", (int)(sizeof(ImDrawIdx) * 8));
    ImGui::BulletText("ImGuiBackendFlags_RendererHasVtxOffset: %s", (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) ? "True" : "False");
    ImGui::Unindent();
    ImGui::Unindent();
    ImGui::Separator();
    ImGui::Text("USER GUIDE:");
    ShowUserGuide();
}

//-----------------------------------------------------------------------------

void ButtonSelector(const char* label, ImGuiMouseButton* b) {
    ImGui::PushID(label);
    if (ImGui::RadioButton("LMB",*b == ImGuiMouseButton_Left))
        *b = ImGuiMouseButton_Left;
    ImGui::SameLine();
    if (ImGui::RadioButton("RMB",*b == ImGuiMouseButton_Right))
        *b = ImGuiMouseButton_Right;
    ImGui::SameLine();
    if (ImGui::RadioButton("MMB",*b == ImGuiMouseButton_Middle))
        *b = ImGuiMouseButton_Middle;
    ImGui::PopID();
}

void ModSelector(const char* label, ImGuiModFlags* k) {
    ImGui::PushID(label);
    ImGui::CheckboxFlags("Ctrl", (unsigned int*)k, ImGuiModFlags_Ctrl); ImGui::SameLine();
    ImGui::CheckboxFlags("Shift", (unsigned int*)k, ImGuiModFlags_Shift); ImGui::SameLine();
    ImGui::CheckboxFlags("Alt", (unsigned int*)k, ImGuiModFlags_Alt); ImGui::SameLine();
    ImGui::CheckboxFlags("Super", (unsigned int*)k, ImGuiModFlags_Super);
    ImGui::PopID();
}

void InputMapping(const char* label, ImGuiMouseButton* b, ImGuiModFlags* k) {
    ImGui::LabelText("##","%s",label);
    if (b != NULL) {
        ImGui::SameLine(100);
        ButtonSelector(label,b);
    }
    if (k != NULL) {
        ImGui::SameLine(300);
        ModSelector(label,k);
    }
}

void ShowInputMapping() {
    ImPlotInputMap& map = ImPlot::GetInputMap();
    InputMapping("Pan",&map.Pan,&map.PanMod);
    InputMapping("Fit",&map.Fit,NULL);
    InputMapping("Select",&map.Select,&map.SelectMod);
    InputMapping("SelectHorzMod",NULL,&map.SelectHorzMod);
    InputMapping("SelectVertMod",NULL,&map.SelectVertMod);
    InputMapping("SelectCancel",&map.SelectCancel,NULL);
    InputMapping("Menu",&map.Menu,NULL);
    InputMapping("OverrideMod",NULL,&map.OverrideMod);
    InputMapping("ZoomMod",NULL,&map.ZoomMod);
    ImGui::SliderFloat("ZoomRate",&map.ZoomRate,-1,1);
}

void Demo_Config() {
    ImGui::ShowFontSelector("Font");
    ImGui::ShowStyleSelector("ImGui Style");
    ImPlot::ShowStyleSelector("ImPlot Style");
    ImPlot::ShowColormapSelector("ImPlot Colormap");
    ImPlot::ShowInputMapSelector("Input Map");
    ImGui::Separator();
    ImGui::Checkbox("Use Local Time", &ImPlot::GetStyle().UseLocalTime);
    ImGui::Checkbox("Use ISO 8601", &ImPlot::GetStyle().UseISO8601);
    ImGui::Checkbox("Use 24 Hour Clock", &ImPlot::GetStyle().Use24HourClock);
    ImGui::Separator();
    if (ImPlot::BeginPlot("Preview")) {
        static double now = (double)time(0);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimits(ImAxis_X1, now, now + 24*3600);
        for (int i = 0; i < 10; ++i) {
            double x[2] = {now, now + 24*3600};
            double y[2] = {0,i/9.0};
            ImGui::PushID(i);
            ImPlot::PlotLine("##Line",x,y,2);
            ImGui::PopID();
        }
        ImPlot::EndPlot();
    }
}

//-----------------------------------------------------------------------------


void Demo_RealtimePlots_FPS(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, streamingStatistics->captureFps);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Game FPS", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "fps", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::PlotLine("Game FPS", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_Latency(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, (int)streamingStatistics->e2erealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("E2E Latency", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("E2E Latency", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_FSize(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, (int)streamingStatistics->framesizerealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Frame Size", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "Bytes/Frame", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Frame Size", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_FDelay(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, (int)streamingStatistics->framedelayrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Frame Delay", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Frame Delay", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_PLoss(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, (int)streamingStatistics->packetlossrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Packet Loss", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "%", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Packet Loss", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_FTime(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, (int)streamingStatistics->framedelayrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Frame Time", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 2, std::get<1>(bounds) + 5, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Frame Time", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_CDec(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, streamingStatistics->decrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Client Decode", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 1, std::get<1>(bounds) + 2, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Latency", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_CRen(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, streamingStatistics->crenrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Client Render", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 1, std::get<1>(bounds) + 2, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Latency", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_SRen(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, streamingStatistics->srenrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Server Render", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 1, std::get<1>(bounds) + 2, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Latency", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

void Demo_RealtimePlots_SEnc(StreamingStatistics* streamingStatistics) {
    static ScrollingBuffer sdata1;
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;
    sdata1.AddPoint(t, streamingStatistics->encrealtime);

    static float history = 10.0f;
    std::tuple<int, int> bounds = sdata1.GetMinMaxY(t - history, t);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    ImPlot::BeginPlot("Server Encode", ImVec2(-1, 150));
    ImPlot::SetupAxes(NULL, "ms", flags, NULL);
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, std::get<0>(bounds) - 1, std::get<1>(bounds) + 2, ImGuiCond_Always);
    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3);
    ImPlot::PlotLine("Latency", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), 0, sdata1.Offset, 2 * sizeof(float));
    ImPlot::EndPlot();
}

//-----------------------------------------------------------------------------
// DEMO WINDOW
//-----------------------------------------------------------------------------

void DemoHeader(const char* label, StreamingStatistics* streamingStatistics, void(*demo)(StreamingStatistics* streamingStatistics)) {
    if (ImGui::TreeNodeEx(label)) {
        demo(streamingStatistics);
        ImGui::TreePop();
    }
}

void ShowImplotWindow(StreamingStatistics* streamingStatistics) {
    static StreamingStatistics g_displayed_stats = { 0 };
    static std::chrono::steady_clock::time_point g_last_display_time;

    std::chrono::steady_clock::time_point now = high_resolution_clock::now();
    auto ms_since_last_update = duration_cast<milliseconds>(now - g_last_display_time).count();

    //Update stats every 120 times a minute to maintain readability.
    //Real time stats can be acquired in either the plots or the log files.
    if (ms_since_last_update >= 500) {
        memcpy(&g_displayed_stats, streamingStatistics, sizeof(g_displayed_stats));
        g_last_display_time = high_resolution_clock::now();
    }

    ImGui::Begin("Statistics", NULL, ImGuiWindowFlags_MenuBar);
    ImGui::Text("Game FPS %3d fps", g_displayed_stats.captureFps);
    ImGui::Text("Captured Frame Width: %d", g_displayed_stats.framewidth);
    ImGui::Text("Captured Frame Height: %d", g_displayed_stats.frameheight);
    ImGui::Text("Client Render: %.2lf ms", g_displayed_stats.crenrealtime);
    ImGui::Text("Client Decode: %.2lf ms", g_displayed_stats.decrealtime);
    ImGui::Text("Server Render: %.2lf ms", g_displayed_stats.srenrealtime);
    ImGui::Text("Server Encode: %.2lf ms", g_displayed_stats.encrealtime);
    ImGui::Text("E2E Latency: %.2lf ms", g_displayed_stats.e2erealtime);
    ImGui::Text("Frame Size: %.2lf bytes", g_displayed_stats.framesizerealtime);
    ImGui::Text("Frame Delay: %.2lf ms", g_displayed_stats.framedelayrealtime);
    ImGui::Text("Packet Loss: %.2lf%%", g_displayed_stats.packetlossrealtime);
    DemoHeader("FPS Plot", streamingStatistics, Demo_RealtimePlots_FPS);
    DemoHeader("Client Render Plot", streamingStatistics, Demo_RealtimePlots_CRen);
    DemoHeader("Client Decode Plot", streamingStatistics, Demo_RealtimePlots_CDec);
    DemoHeader("Server Render Plot", streamingStatistics, Demo_RealtimePlots_SRen);
    DemoHeader("Server Encode Plot", streamingStatistics, Demo_RealtimePlots_SEnc);
    DemoHeader("E2E Latency Plot", streamingStatistics, Demo_RealtimePlots_Latency);
    DemoHeader("Frame Size Plot", streamingStatistics, Demo_RealtimePlots_FSize);
    DemoHeader("Frame Delay Plot", streamingStatistics, Demo_RealtimePlots_FDelay);
    DemoHeader("Packet Loss Plot", streamingStatistics, Demo_RealtimePlots_PLoss);
    DemoHeader("Frame Time Plot", streamingStatistics, Demo_RealtimePlots_FTime);

    ImGui::End();
}

} // namespace ImPlot
