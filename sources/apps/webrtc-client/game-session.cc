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

// clang-format off
#include "game-session.h"
#include "peer-connection.h"

// clang-format on
GameSession::GameSession() {
  pc_.reset(new PeerConnection());
  prev_pointerlock_status_ = false;
  render_width_  = 0;
  render_height_ = 0;
}

GameSession::~GameSession() {
  if( NULL != cursor_data_ )
  {
    free(cursor_data_);
    cursor_data_ = NULL;
  }
}

void GameSession::OnStreamAdded()
{
  // start negotiation with server.

  // Step1: report client display resolution to server
  if (render_width_ !=0 && render_height_ != 0 ) {//TODO: need to add protocol between server and client for the negotiate
      std::string m;
      m = InputEventHandler::OnSizeChange(render_width_, render_height_);
      pc_->SendMessage(m);
  }
  //
}
void GameSession::SendSizeChange(UINT render_w, UINT render_h) {

  if (render_w != render_width_ || render_h != render_height_) {
      render_width_  = render_w;
      render_height_ = render_h;
  }
}

void GameSession::OnDataReceivedHandler(
  const std::string &message) {
  rapidjson::Document msg;

  if (!ga::json::ParseMessage(msg, message)) {
    ga::log::WriteToFile("ClientErrorLog", "txt", "[%s][%lu][WARNING]: Failed to parse message: %s\n", __FUNCTION__, __LINE__, message.c_str());
    return;
  }

  std::string type = ga::json::FromString(msg, "type");
  if (type == "cursor") {
    if (connect_settings_.mousestate_callback_) {
      struct ga::remote::CursorInfo cursorInfo = {0};
      //receive cursor info
      cursorInfo.isVisible = ga::json::FromBool(msg, "visible");
      if (cursorInfo.isVisible) {
          cursorInfo.width  = ga::json::FromUint64(msg, "width");
          cursorInfo.height = ga::json::FromUint64(msg, "height");
          cursorInfo.pitch  = ga::json::FromUint64(msg, "pitch");
          cursorInfo.cursorDataUpdate = !ga::json::FromBool(msg, "noShapeChange");
          if (cursorInfo.cursorDataUpdate) {
            cursorInfo.cursordata.clear();
            if (ga::json::MemberType(msg, "cursorData") == rapidjson::Type::kArrayType && msg["cursorData"].Size() <= MAX_CURSOR_SIZE) {
              for (auto it = msg["cursorData"].Begin(); it != msg["cursorData"].End(); ++it) {
                  cursorInfo.cursordata.push_back(it->GetUint());
              }
            } else {
              //TODO: current server maximum is 64x64x4
            }
          }
        }
        connect_settings_.mousestate_callback_(cursorInfo);
    }
  }
}

void GameSession::SendPointerlockchange(bool relativeMode)
{
  if (prev_pointerlock_status_ != relativeMode) {
    std::string m;
    m = InputEventHandler::onPointerlockchange(relativeMode);
    pc_->SendMessage(m);
    prev_pointerlock_status_ = relativeMode;
  }
}

void CALLBACK GameSession::SendFrameStats(FrameStats *p_frame_stats)
{

  std::string m;
  m = InputEventHandler::OnStatsRequest(p_frame_stats);
  pc_->SendMessage(m);
}

void GameSession::SendMouseEvent(MouseOptions *p_m_options, bool is_raw) {
  std::string m;
  m = InputEventHandler::OnMouseEvent(p_m_options, is_raw);
  pc_->SendMessage(m);
}

void GameSession::SendKeyboardEvent(
    KeyboardOptions *p_key_options) {
  std::string m;
  m = InputEventHandler::OnKeyboardEvent(p_key_options);
  pc_->SendMessage(m);
}

int GameSession::ConnectPeerServer(StreamingStatistics* streamingStatistics) {
  pc_->Init(session_id_);
  pc_->SetWindowHandle(connect_settings_.hwnd_);
  pc_->SetWindowSize(0, 0, GetSystemMetrics(SM_CXSCREEN),
      GetSystemMetrics(SM_CYSCREEN));
  if (streamingStatistics) {
      pc_->setStreamingStatistics(streamingStatistics);
  }
  pc_->Connect(peer_server_url_, session_id_, client_id_);
  pc_->Start();
  return 0;
}

void GameSession::ConfigConnection(const ga::remote::SessionMetaData &session_info,
  const ga::remote::ClientSettings client_settings) {
  int error_code = 0;
  peer_server_url_ = session_info.peer_server_url_;
  session_id_ = session_info.session_id_;
  client_id_ = session_info.client_id_;
  connect_settings_.mousestate_callback_ = client_settings.mousestate_callback_;
  connect_settings_.connection_callback_ = client_settings.connection_callback_;
  connect_settings_.hwnd_ = client_settings.hwnd_;
  pc_->session_ = this;
}

int GameSession::OnServerConnected(string &game_session_id) {
  int err_num = 0;
  if (connect_settings_.connection_callback_) {
    err_num = connect_settings_.connection_callback_(game_session_id);
  }
  else {
  }
  return err_num;
}

int GameSession::StopConnection(void) {
  pc_->Stop();
  return 0;
}

void GameSession::SetWindowSize(UINT x_offset, UINT y_offset,
                                            UINT width, UINT height) {
  pc_->SetWindowSize(x_offset, y_offset, width, height);
}
