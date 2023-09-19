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

#ifndef GA_OPTION_H_
#define GA_OPTION_H_

#include <functional>
#include <string>
#include <vector>
#include <windows.h>
#include "statistics-window-class.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using std::function;
using std::string;

#define MAX_CURSOR_SIZE 64*64*4
#define MAX_LOG_BUFFER_SIZE 1024

namespace ga {
  namespace remote {
    struct CursorInfo {
      UINT type;
      BOOL isVisible;
      LONG pos_x;
      LONG pos_y;
      UINT width;
      UINT height;
      UINT pitch;
      UINT cursorDataUpdate;
      std::vector<uint8_t> cursordata;
    };

    typedef function<int(string &)>             ConnectionCallback;
    typedef function<void(struct CursorInfo &)> MouseStateCallback;

    struct SessionMetaData {
      string session_id_;
      string client_id_;
      string peer_server_url_;
    };

    // All options for starting the client. Provides interfaces for client application
    // to register all necessary callbacks to handle audio/video and register callbacks
    // for delivering input, as well as cursor things, etc.
    struct ClientSettings {
      HWND hwnd_;
      ConnectionCallback connection_callback_;
      MouseStateCallback mousestate_callback_;
    };

    int  StartGame(const SessionMetaData &session_opts, const ClientSettings opts, StreamingStatistics* streamingStatistics);
    int  ExitGame(const string &sessionid);
    void SendInput(UINT input_message, WPARAM w_param, LPARAM l_param);
    void SetWindowSize(UINT x_offset, UINT y_offset, UINT width, UINT height);
    void ChangeCursorReportMode(bool RelativeMode);
  } // namespace remote

  namespace log {
    FILE* OpenFile(std::string fileName, std::string fileType);
    void  WriteToMsg(std::string &logMsg, const char *format_string, ...);
    bool  FlushMsgToFile(FILE *destFile, std::string &logMsg);
    void  CloseFile(FILE *file);
    bool  WriteToFile(std::string fileName, std::string fileType, const char *format_string, ...);
  } // namespace log

  namespace json {
    bool            ParseMessage(rapidjson::Document& document, const std::string& message);
    rapidjson::Type MemberType(const rapidjson::Document& document, const std::string& message);
    uint64_t        FromUint64(const rapidjson::Document& document, const std::string& message);
    std::string     FromString(const rapidjson::Document& document, const std::string& message);
    bool            FromBool(const rapidjson::Document& document, const std::string& message);
  } // namespace json
} // namespace ga

extern std::string FLAGS_peer_server_url;
extern std::string FLAGS_sessionid;
extern std::string FLAGS_clientid;
extern bool FLAGS_show_statistics;
extern bool FLAGS_logging;
extern bool FLAGS_streamdump;
extern bool FLAGS_enable_rext;
extern bool FLAGS_verbose;
extern std::string FLAGS_stunsvr;

#endif GA_OPTION_H_
