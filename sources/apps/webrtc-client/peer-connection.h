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

#ifndef GA_PEERCONNECTION_H_
#define GA_PEERCONNECTION_H_

#include "video-renderer.h"
#include "owt/base/commontypes.h"
//#include "owt/base/connectionstats.h"
#include "owt/base/globalconfiguration.h"
#include "owt/base/logging.h"
#include "owt/base/network.h"
#include "owt/base/videorendererinterface.h"
#include "owt/p2p/p2pclient.h"
#include "rtc-signaling.h"
#include <memory>

using owt::base::GlobalConfiguration;
using owt::p2p::P2PClient;
using owt::p2p::P2PClientObserver;

class GameSession;

/// This class is the webrtc transport wrapper for gaming client.
class PeerConnection : public P2PClientObserver {
public:
  PeerConnection() : stream_started_(false) {}
  void Init(const std::string &session_token);
  void Connect(const std::string &peer_server_url,
               const std::string &session_token,
               const std::string &client_id);
  void SetWindowHandle(HWND hwnd);
  void Start();
  void Stop();
  void SendMessage(const std::string &msg);
  void SetWindowSize(UINT x, UINT y, UINT w, UINT h) {
    dx_renderer_.SetWindowSize(x, y, w, h);
  }
  void setStreamingStatistics(StreamingStatistics* streamingStatistics) { dx_renderer_.setStreamingStatistics(streamingStatistics); };
  GameSession* session_;

protected:
  // PeerClientObserver impl
  void OnStreamAdded(std::shared_ptr<RemoteStream> stream) override;
  void OnMessageReceived(const std::string &remote_user_id,
    const std::string message)override;
private:
  std::shared_ptr<P2PClient> pc_;
  std::shared_ptr<RemoteStream> remote_stream_;
  owt::base::VideoRenderWindow render_window_;
  std::shared_ptr<P2PSignalingChannel> signaling_channel_;
  DXRenderer dx_renderer_;
  std::string remote_peer_id_;
  static long send_invoke_is_safe_;
  static long send_success_;
  bool stream_started_;
  static long send_timeout_;
  bool connection_active_;
};
#endif // GA_PEERCONNECTION_H_
