// Copyright (C) 2022 Intel Corporation
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

#pragma once

#include <future>

#include "p2p-socket-signaling-channel.h"

#ifdef WIN32
#include "ga-controller-sdl.h"
#include "ga-audio-input.h"
#else
#include "ga-controller-android.h"
#endif
#include "ga-video-input.h"
#include "ga-module.h"

#ifndef WIN32
#include "aic-vhal-client/RemoteStreamHandler.h"
#include "aic-vhal-client/EncodedVideoDispatcher.h"
#include "aic-vhal-client/SensorHandler.h"
#include "libvhal/virtual_gps_receiver.h"
#include "aic-vhal-client/CommandChannelHandler.h"
#include "aic-vhal-client/CameraClientHandler.h"
#endif

#include "owt/base/clock.h"
#include "owt/base/globalconfiguration.h"
#include "owt/p2p/p2pclient.h"
#ifdef E2ELATENCY_TELEMETRY_ENABLED
#ifndef WIN32
#include <climits>
#endif
#endif

using namespace owt::base;
using namespace owt::p2p;

namespace ga {
namespace webrtc {
class ICSP2PClient : public owt::p2p::P2PClientObserver,
                     public owt::base::EncoderObserver,
                     public PublicationObserver,
                     public std::enable_shared_from_this<ga ::webrtc ::ICSP2PClient> {
 public:
  ~ICSP2PClient() = default;

  ICSP2PClient& operator=(const ICSP2PClient&) = delete;
#ifdef WIN32
  GAAudioFrameGenerator* audioGenerator;
#endif

  int32_t      Init(void *arg);
  void         Deinit();
  int32_t      Start();
  void         InsertFrame(ga_packet_t *packet);
  void         SendCursor(std::shared_ptr<CURSOR_DATA> cursor_data);
  void         SendQoS(std::shared_ptr<QosInfo> qos_info);
  // Returns the bytes sent by the server since last call.
  int64_t      GetCreditBytes();
  int64_t      GetMaxBitrate();
  //Encoder observer impl.
  void         OnStarted();
  void         OnStopped();
  void         OnKeyFrameRequest();
  void         OnRateUpdate(uint64_t bitrate_bps, uint32_t frame_rate);
  // Publication observer
  void         OnEnded();
  void         OnMute(TrackKind track_kind) {}
  void         OnUnmute(TrackKind track_kind) {}
  virtual void OnError(std::unique_ptr<Exception> failure) {}
#ifdef E2ELATENCY_TELEMETRY_ENABLED
  // E2Elatency
  uint32_t UpdateFrameNumber() {
    if (frame_number_ > ((1ULL << sizeof(uint32_t) * CHAR_BIT) - 1))
      frame_number_ = 0; // roll-over
    else
      ++frame_number_; // increment

    return frame_number_;
  }

  uint32_t GetFrameNumber() const {
    return frame_number_;
  }

  void     HandleLatencyMessage(uint64_t latency_send_time_ms);
#endif
protected:
  virtual void OnMessageReceived(const std::string &remote_user_id,
                              const std::string message) override;
  virtual void OnStreamAdded(std::shared_ptr<owt::base::RemoteStream> stream) override;
  virtual void OnPeerConnectionClosed(const std::string& remote_user_id) override;
  virtual void OnLossNotification(DependencyNotification notification) override {};

private:
  void RegisterCallbacks();
  void ConnectCallback(bool is_fail, const std::string &error);
  void CreateStream();
  void RequestCursorShape();

  std::shared_ptr<owt::p2p::P2PClient>    p2pclient_;
  std::shared_ptr<owt::base::LocalStream> local_stream_;
  std::shared_ptr<owt::base::LocalStream> local_audio_stream_;
  std::shared_ptr<owt::p2p::Publication>   publication_;
#ifndef WIN32
  std::shared_ptr<RemoteStreamHandler>   remote_stream_handler_;
  std::unique_ptr<SensorHandler>         sensor_handler_;
  std::shared_ptr<CameraClientHandler>   camera_client_handler_;
  std::unique_ptr<VirtualGpsReceiver>    virtual_gps_receiver_;
  std::unique_ptr<CommandChannelHandler> command_channel_handler_;
#endif
  std::shared_ptr<owt::base::EncodedStreamProvider> stream_provider_;
  std::promise<int32_t>                             connect_status_;
  std::shared_ptr<GAVideoEncoder>                   ga_encoder_;
  std::unique_ptr<Controller>                       controller_;

  int64_t bytes_sent_on_last_stat_call_   = 0;
  int64_t bytes_sent_on_last_credit_call_ = 0;
  int64_t credit_bytes_                   = 0;
  int64_t current_available_bandwidth_    = 0;

  std::unique_ptr<owt::base::Clock> clock_;

  std::string remote_user_id_;
  bool        streaming_         = false;
  uint8_t     cursor_shape_[4096];  // The latest cursor shape.
  bool        first_cursor_info_ = false;
  bool        capturer_started_  = false;
  bool        enable_dump_       = false;
  FILE*       dump_file_         = nullptr;
  uint64_t    last_timestamp_    = 0;
  uint64_t    send_failures_     = 0;
  bool        send_blocked_      = true;
#ifdef E2ELATENCY_TELEMETRY_ENABLED
  // E2ELatency
  bool HasClientStats() const { return client_latency_.send_time_ms != 0; }

  uint32_t frame_number_;     // Current frame number that we are processing.
  uint32_t frame_delay_ = 1;  // Delay latency response to client input.
                              // Attempts to ensure result of the client input is "included" in the rendered frame.

  struct {
    uint64_t send_time_ms          = 0;  // Time client sent latency message to server.
                                         // Measured in miliseconds since the epoch.

    uint64_t received_time_ms      = 0;  // Time when server received latency message from client.
                                         // Measured in miliseconds since the epoch.

    uint32_t received_frame_number = 0;  // Frame number when latency info was received from client.
  } client_latency_;
#endif
  bool enable_render_drc_       = false;

  std::function<void(bool)> hook_client_status_function_;
};
} // namespace webrtc
} // namespace ga
