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

#include <iostream>
#include <windows.h>
#include <stdint.h>
#include "window-handler.h"
#include "ga-option.h"
#include <map>
#include "statistics-window-class.h"

std::string FLAGS_peer_server_url = "";
std::string FLAGS_sessionid = "ga";
std::string FLAGS_clientid = "client";
bool FLAGS_show_statistics = false;
bool FLAGS_logging = false;
bool FLAGS_streamdump = false;
bool FLAGS_verbose = false;
std::string FLAGS_stunsvr = "stun:stun.l.google.com:19302";

void Usage(std::string cmd)
{
  MessageBox(
    NULL,
    L"See client section in WCG README for full list of options",
    L"Usage",
    MB_OK
  );
}

void ParseCommandLineFlags(int argc, char** argv)
{
  for (int idx = 1; idx < argc; ++idx) {
    if (std::string("-h") == argv[idx] ||
      std::string("--help") == argv[idx]) {
      Usage(argv[0]);
      exit(0);
    } else if (std::string("--peer_server_url") == argv[idx]) {
      if (++idx >= argc) break;
      FLAGS_peer_server_url = argv[idx];
    } else if (std::string("--sessionid") == argv[idx]) {
      if (++idx >= argc) break;
      FLAGS_sessionid = argv[idx];
    } else if (std::string("--clientid") == argv[idx]) {
      if (++idx >= argc) break;
      FLAGS_clientid = argv[idx];
    } else if (std::string("--show_statistics") == argv[idx]) {
      FLAGS_show_statistics = true;
    } else if (std::string("--logging") == argv[idx]) {
      FLAGS_logging = true;
    } else if (std::string("--streamdump") == argv[idx]) {
      FLAGS_streamdump = true;
    } else if (std::string("--verbose") == argv[idx]) {
      FLAGS_verbose = true;
    } else if (std::string("--stunsvr") == argv[idx]) {
      if (++idx >= argc) break;
      FLAGS_stunsvr = argv[idx];
    } else {
      break;
    }
  }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
  LPSTR lpCmdLine, int nCmdShow) {
  MSG Msg;
  ga::remote::SessionMetaData session_info;
  ga::remote::ClientSettings client_settings;
  WindowHandler *window_handler = nullptr;
  bool is_done = false;
  std::unique_ptr<StatisticsWindowClass> swc_ = nullptr;
  StreamingStatistics streamingStatistics;

  ParseCommandLineFlags(__argc, __argv);
  if (FLAGS_show_statistics) {
    FLAGS_verbose = true;
  }
  session_info.peer_server_url_ = FLAGS_peer_server_url;
  session_info.session_id_ = FLAGS_sessionid;
  session_info.client_id_ = FLAGS_clientid;

  client_settings.mousestate_callback_ = WindowHandler::OnMouseStateChange;
  client_settings.connection_callback_ = WindowHandler::OnGameServerConnected;

  if ((window_handler = WindowHandler::GetInstance()) == nullptr) {
    goto Done;
  }

  if (window_handler->InitializeGameWindow(hInstance, nCmdShow,"GaWebRTCClient") != 0) {
    goto Done;
  }

  client_settings.hwnd_ = window_handler->GetWindowHandle();
  if (FLAGS_show_statistics) {
    swc_.reset(new StatisticsWindowClass(hInstance, nCmdShow));
    streamingStatistics.init();
    swc_->setStreamingStatistics(&streamingStatistics);
    ga::remote::StartGame(session_info, client_settings, &streamingStatistics);
  } else if (FLAGS_verbose) {
    streamingStatistics.init();
    ga::remote::StartGame(session_info, client_settings, &streamingStatistics);
  } else {
    ga::remote::StartGame(session_info, client_settings, nullptr);
  }

  /*while (GetMessage(&Msg, NULL, 0, 0) > 0) {
    TranslateMessage(&Msg);
    DispatchMessage(&Msg);
  }*/
  while (!is_done) {
    if (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    if (WM_QUIT == Msg.message) {
        is_done = true;
    }

    if (FLAGS_show_statistics && swc_ && streamingStatistics.updated) {
        streamingStatistics.updated = false;
        swc_->DrawStatistics();
    }
  }

Done:
  if (window_handler) {
    window_handler->Destroy();
  }

  if (FLAGS_show_statistics && swc_) {
      swc_->Destroy();
      swc_ = NULL;
  }

  return Msg.wParam;
}
