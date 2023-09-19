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
#include "control-handler.h"
#include "peer-connection.h"
#include "game-session.h"
// clang-format on

using owt::base::GlobalConfiguration;
using owt::p2p::P2PClient;
using owt::p2p::P2PClientObserver;

long PeerConnection::send_invoke_is_safe_ = 1;
long PeerConnection::send_success_ = 0;
long PeerConnection::send_timeout_ = 30;

void PeerConnection::Init(const std::string &session_token) {
  if (FLAGS_logging) {
    // Typically this will output log in msys2 console. Not sure why CMD shell does not...
    owt::base::Logging::Severity(owt::base::LoggingSeverity::kInfo);
    owt::base::Logging::LogToConsole(owt::base::LoggingSeverity::kInfo);
  }
  signaling_channel_.reset(new P2PSignalingChannel());
  GlobalConfiguration::SetLowLatencyStreamingEnabled(true);
  if (FLAGS_streamdump)
    GlobalConfiguration::SetPreDecodeDumpEnabled(true);
  if (FLAGS_enable_rext)
    GlobalConfiguration::SetRangeExtensionEnabled(true);

  owt::p2p::P2PClientConfiguration configuration_;
  owt::base::IceServer icesvr;
  icesvr.urls.push_back(FLAGS_stunsvr);
  configuration_.ice_servers.push_back(icesvr);
  AudioCodecParameters audio_codec_params;
  audio_codec_params.name = AudioCodec::kOpus;
  AudioEncodingParameters audio_params(audio_codec_params, 0);
  configuration_.audio_encodings.push_back(audio_params);
  pc_.reset(new P2PClient(configuration_, signaling_channel_));
  pc_->AddObserver(*this);
  remote_peer_id_ = session_token;
  pc_->AddAllowedRemoteId(remote_peer_id_);
}

void PeerConnection::SetWindowHandle(HWND hwnd) {
  render_window_.SetWindowHandle(hwnd);
  // Initialize DX container
  dx_renderer_.SetWindow(hwnd);
}

// Johny: currently this is hard-coded to use id "client"
void PeerConnection::Connect(const std::string &peer_server_url,
                             const std::string &session_token,
                             const std::string &client_id) {
  std::string token = client_id;
  pc_->Connect(
    peer_server_url, token,
    [this](const std::string &id) {
      // TODO: we should add some handling here... Otherwise it might crash sending the start message.
    },
    [this](std::unique_ptr<Exception> err) {
    });
}

void PeerConnection::Start() {
  long send_success_local = 0;

  while (!send_success_local) {
    if (send_invoke_is_safe_) {
      InterlockedExchange(&send_invoke_is_safe_, 0);
      pc_->Send(
          remote_peer_id_, "start",
          [&](void) {
            InterlockedExchange(&send_success_, 1);
            InterlockedExchange(&send_invoke_is_safe_, 1);
            if (session_) {
              session_->OnServerConnected(remote_peer_id_);
            }
          },
          [&](std::unique_ptr<Exception> err) {
            err.release();
            InterlockedExchange(&send_invoke_is_safe_, 1);
          });
    }
    Sleep(1000);
    InterlockedExchange(&send_success_local, send_success_);
  }
  connection_active_ = true;
}

void PeerConnection::Stop() {
  dx_renderer_.Cleanup();
  connection_active_ = false;
}

void PeerConnection::SendMessage(const std::string &msg) {
  if (stream_started_) {
    pc_->Send(remote_peer_id_, msg, nullptr, nullptr);
  }
}

// Johny: this is the interface WebRTC receives video/audio streams.
void PeerConnection::OnStreamAdded(std::shared_ptr<RemoteStream> stream) {
  stream_started_ = true;
  remote_stream_ = stream;
  remote_stream_->AttachVideoRenderer(dx_renderer_);
  session_->OnStreamAdded();
}


void PeerConnection::OnMessageReceived(const std::string &remote_user_id,
  const std::string message) {
  if (session_) {
    session_->OnDataReceivedHandler(message);
  }
}
