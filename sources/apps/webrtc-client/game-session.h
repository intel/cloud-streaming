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

#ifndef GA_REMOTE_CONECTION_HANDLER_H_
#define GA_REMOTE_CONECTION_HANDLER_H_

#include "peer-connection.h"
#include "ga-option.h"
#include "stdlib.h"
#include "control-handler.h"
#include <memory>

#include <Windows.h>

class GameSession {
public:
  explicit GameSession();
  ~GameSession();
  void OnDataReceivedHandler(const std::string &message);
  int OnServerConnected(string &session_id);
  void ConfigConnection(const ga::remote::SessionMetaData &opts, const ga::remote::ClientSettings launch_options);
  void SendSizeChange(UINT render_w, UINT render_h);
  void SendMouseEvent(MouseOptions *p_m_options, bool is_raw);
  void SendKeyboardEvent(KeyboardOptions *p_key_options);
  int ConnectPeerServer(StreamingStatistics* streamingStatistics);
  int StopConnection(void);
  void SetWindowSize(UINT x_offset, UINT y_offset, UINT width, UINT height);
  void OnStreamAdded();
  void SendPointerlockchange(bool change);

  void CALLBACK SendFrameStats(FrameStats* p_frame_stats);

private:
  std::string session_id_;
  std::string client_id_;
  std::string peer_server_url_;
  std::unique_ptr<PeerConnection> pc_;
  ga::remote::ClientSettings connect_settings_;
  char *cursor_data_;  // Store the cursor data
  UINT render_width_;  // local display resoluiton width
  UINT render_height_; // local display resoluiton height
  BOOL prev_pointerlock_status_; // track client cursor mode change between absolute and relatie mode
  void* gpMsg_ = nullptr;
};
#endif
