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

#include "ics-p2p-client.h"
#include "ga-video-input.h"
#include "ga-conf.h"
#include "ga-cursor.h"
#include "ga-qos.h"
#include "encoder-common.h"
#ifdef WIN32
#include "ga-audio-input.h"
#else
#include "android-common.h"
#include "audio-frame-generator.h"
#include "audio_common.h"
#include "video_sink.h"
#endif
#include "rtspconf.h"
#include <iostream>

#include "owt/base/deviceutils.h"
#include "owt/base/stream.h"
#include "owt/base/logging.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include "nlohmann/json.hpp"

using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::chrono::milliseconds;

#ifdef WIN32
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dmoguids.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "msdmo.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#endif

using namespace ga::webrtc;

// If send fails 100 times consecutively, block sending
// cursor and QoS info until receiving further message from
// client.
#define OWT_MAX_SEND_FAILURES 100

static const bool g_bEnableOwtStats = false;

static std::string get_p2p_server() {
  std::string host = ga_conf_readstr("signaling-server-host");
  std::string port = ga_conf_readstr("signaling-server-port");

  if (host.empty()) {
    host = "127.0.0.1";
    ga_logger(Severity::INFO, "*** no signaling server host specified, default to %s.\n", host.c_str());
  }
  if (port.empty()) {
    port = "8095";
    ga_logger(Severity::INFO, "*** no signaling server port specified, default to %s.\n", port.c_str());
  }

  std::string p2p = "http://" + host + ":" + port;
  return p2p;
}

static int32_t get_android_session() {
  int32_t session = -1;
  if (ga_conf_readbool("k8s", 0) == 0) {
    session = ga_conf_readint("android-session");
    if (session < 0)
      session = 0;
  }
  return session;
}

void ICSP2PClient::RegisterCallbacks() {
#ifndef WIN32
  int32_t session = get_android_session();

  auto cmd_handler = [this](uint32_t cmd) {
    if (cmd == vhal::client::audio::Command::kOpen) {
      p2pclient_->Send(remote_user_id_, RemoteStreamHandler::startAudioRecMsg.c_str(),
                       nullptr, nullptr);
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
              RemoteStreamHandler::startAudioRecMsg.c_str());

    } else if (cmd == vhal::client::audio::Command::kClose &&
               remote_stream_handler_->HasActiveStream()) {
      p2pclient_->Send(remote_user_id_, RemoteStreamHandler::stopAudioRecMsg.c_str(),
                       nullptr, nullptr);
      remote_stream_handler_->unSubscribeForAudio();
      remote_stream_handler_->resetStream();
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
                RemoteStreamHandler::stopAudioRecMsg.c_str());
    } else if (cmd == vhal::client::audio::Command::kStartstream) {
      p2pclient_->Send(remote_user_id_, RemoteStreamHandler::startAudioPlayMsg.c_str(),
                       nullptr, nullptr);
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
              RemoteStreamHandler::startAudioPlayMsg.c_str());

    } else if (cmd == vhal::client::audio::Command::kStopstream) {
      p2pclient_->Send(remote_user_id_, RemoteStreamHandler::stopAudioPlayMsg.c_str(),
                       nullptr, nullptr);
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
              RemoteStreamHandler::stopAudioPlayMsg.c_str());
    } else if (cmd == (uint32_t)vhal::client::VideoSink::camera_cmd_t::CMD_OPEN) {
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
                camera_client_handler_->startPreviewStreamMsg.c_str());
      p2pclient_->Send(remote_user_id_, camera_client_handler_->startPreviewStreamMsg.c_str(),
                       nullptr, nullptr);

    } else if (cmd == (uint32_t)vhal::client::VideoSink::camera_cmd_t::CMD_CLOSE) {
      ga_logger(Severity::INFO, "RemoteCmd Send message: %s\n",
                camera_client_handler_->stopPreviewStreamMsg.c_str());
      p2pclient_->Send(remote_user_id_, camera_client_handler_->stopPreviewStreamMsg.c_str(),
                       nullptr, nullptr);

    } else if (cmd == SensorHandler::Command::kSensorStart) {
      ga_logger(Severity::INFO, "Send message: %s\n",
           SensorHandler::sensorStartMsg.c_str());
      p2pclient_->Send(remote_user_id_, SensorHandler::sensorStartMsg.c_str(),
                       nullptr, nullptr);
    } else if (cmd == SensorHandler::Command::kSensorStop) {
      ga_logger(Severity::INFO, "Send message: %s\n",
              SensorHandler::sensorStopMsg.c_str());
      p2pclient_->Send(remote_user_id_, SensorHandler::sensorStopMsg.c_str(),
                       nullptr, nullptr);
    } else if (cmd == VirtualGpsReceiver::Command::kGpsStart) {
      ga_logger(Severity::INFO, "Send message: %s\n",
              VirtualGpsReceiver::gpsStartMsg.c_str());
      p2pclient_->Send(remote_user_id_, VirtualGpsReceiver::gpsStartMsg.c_str(),
                       nullptr, nullptr);
    } else if (cmd == VirtualGpsReceiver::Command::kGpsStop) {
      ga_logger(Severity::INFO, "Send message: %s\n",
              VirtualGpsReceiver::gpsStopMsg.c_str());
      p2pclient_->Send(remote_user_id_, VirtualGpsReceiver::gpsStopMsg.c_str(),
                       nullptr, nullptr);
    } else if (cmd == VirtualGpsReceiver::Command::kGpsQuit) {
      ga_logger(Severity::INFO, "Send message: %s\n",
              VirtualGpsReceiver::gpsQuitMsg.c_str());
      p2pclient_->Send(remote_user_id_, VirtualGpsReceiver::gpsQuitMsg.c_str(),
                       nullptr, nullptr);
    } else {
      // not sure now
    }
  };
  std::unique_ptr<AudioFrameGenerator> generator(
    AudioFrameGenerator::Create(get_android_session(), cmd_handler));
  GlobalConfiguration::SetCustomizedAudioInputEnabled(true,
                                                      std::move(generator));
  remote_stream_handler_ = std::make_shared<RemoteStreamHandler>(session, cmd_handler);
  ga_logger(Severity::INFO, "RemoteStreamHandler Created !!!.\n");
  sensor_handler_ = std::make_unique<SensorHandler>(session, cmd_handler);
  ga_logger(Severity::INFO, "SensorHandler Created !!!.\n");

  vhal::client::TcpConnectionInfo conn_info = { android::ip() };

  virtual_gps_receiver_ = std::make_unique<VirtualGpsReceiver>(conn_info, cmd_handler);
  ga_logger(Severity::INFO, "VirtualGpsReceiver Created !!!.\n");

  auto encodedVideoDispatcher = std::make_unique<EncodedVideoDispatcher>(session, cmd_handler);
  camera_client_handler_ = encodedVideoDispatcher->GetCameraClientHandler();
  GlobalConfiguration::SetCustomizedVideoDecoderEnabled(std::move(encodedVideoDispatcher));
  ga_logger(Severity::INFO, "SetCustomizedVideoDecoderEnabled !!!.\n");

  auto cmd_channel_msg_handler =
    [this](vhal::client::MsgType type, const std::string& msg) {
    if (msg.empty())
      return;

    std::string msg_json;
    if (type == vhal::client::MsgType::kActivityMonitor) {
      msg_json = "{\"key\":\"activity-switch\",\"val\":\"" + msg + "\"}";
    } else if (type == vhal::client::MsgType::kAicCommand) {
      msg_json = "{\"key\":\"cmd-output\",\"val\":\"" + msg + "\"}";
    }
    p2pclient_->Send(remote_user_id_, msg_json.c_str(),
                     nullptr, nullptr);
  };
  command_channel_handler_ =
    std::make_unique<CommandChannelHandler>(session, cmd_channel_msg_handler);
#endif
}

#ifdef E2ELATENCY_TELEMETRY_ENABLED
void ICSP2PClient::HandleLatencyMessage(uint64_t latency_send_time_ms) {
  // If we have latency we are waiting to send out, dont update with new values.
  if (HasClientStats()) {
    return;
  }

  // This stat must use system_clock since its used for calculating a statistic across systems.
  // high_resolution_clock does not account for time zone differences and so reports erroneous values.
  // Other stats which are purely local should still use high_resolution_clock.
  client_latency_.received_time_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  client_latency_.send_time_ms = latency_send_time_ms;
  client_latency_.received_frame_number = GetFrameNumber();
}
#endif

int32_t ICSP2PClient::Init(void *arg) {
  memset(cursor_shape_, 0, sizeof(cursor_shape_));
  first_cursor_info_ = true;
  streaming_ = false;

  uint16_t game_width = 1280;
  uint16_t game_height = 720;
#ifdef WIN32
  if (arg != NULL) {
    struct ServerConfig *webrtcCfg = (struct ServerConfig*) arg;
    if (webrtcCfg->pHookClientStatus != NULL) {
      hook_client_status_function_ = webrtcCfg->pHookClientStatus;
    }
    struct gaRect *rect = (struct gaRect*) webrtcCfg->prect;
    game_width = rect->right - rect->left + 1;
    game_height = rect->bottom - rect->top + 1;
  }
#else
  if (ga_conf_readbool("measure-latency", 0) == 1) {
    android::atrace_init();
  }
#endif

#ifdef WIN32
  controller_ = std::make_unique<SdlController>(game_width, game_height, 480, 320);
#else
  controller_ = std::make_unique<AndroidController>(game_width, game_height, 480, 320);
#endif

  // Handle webrtc signaling related settings
  std::string server_peer_id = ga_conf_readstr("server-peer-id");
  std::string client_peer_id = ga_conf_readstr("client-peer-id");

  if (server_peer_id.empty()) {
    server_peer_id = "ga";
    ga_logger(Severity::INFO, "*** no server peer id specified, default to %s.\n", server_peer_id.c_str());
  }
  if (client_peer_id.empty()) {
    client_peer_id = "client";
    ga_logger(Severity::INFO, "*** no client peer specified, default to %s\n", client_peer_id.c_str());
  }

  // When publishing local encoded stream, this must be enabled. Otherwise
  // disabled.
  bytes_sent_on_last_stat_call_ = 0;
  bytes_sent_on_last_credit_call_ = 0;
  current_available_bandwidth_ = 8 * 1000 * 1000;  //Initial setting for 1080p. Will be adjusted.
  GlobalConfiguration::SetEncodedVideoFrameEnabled(true);
  GlobalConfiguration::SetAEC3Enabled(false);
  GlobalConfiguration::SetAECEnabled(false);
  GlobalConfiguration::SetAGCEnabled(false);

  int32_t ice_port_min = ga_conf_readint("ice-port-min");
  int32_t ice_port_max = ga_conf_readint("ice-port-max");
  if ((ice_port_min > 0) && (ice_port_max > 0)) {
    ga_logger(Severity::INFO, "ice_port_min = %ld ice_port_max = %ld\n", ice_port_min, ice_port_max);
    GlobalConfiguration::SetIcePortAllocationRange(ice_port_min, ice_port_max);
  } else {
    ga_logger(Severity::INFO, "*** no ICE port range specified\n");
  }

  // Now by default, video hardware acceleration is enabled. On platform prior
  // to Haswell, need to call SetVideoHardwareAccelerationEnabled(false) to
  // disable hardware acceleration.
  GlobalConfiguration::SetVideoHardwareAccelerationEnabled(true);
  GlobalConfiguration::SetLowLatencyStreamingEnabled(true);
  GlobalConfiguration::SetBweRateLimits(6 * 1024, 512, 24 * 1024);

  // Always enable customized audio input here. `CreateStream` will
  // enable/disable audio track according to conf.
  std::shared_ptr<P2PSocketSignalingChannel> signaling =
     std::make_shared<P2PSocketSignalingChannel>(P2PSocketSignalingChannel());

#ifdef WIN32
  struct RTSPConf* conf = rtspconf_global();
  std::unique_ptr<GAAudioFrameGenerator> generator =
     std::unique_ptr<GAAudioFrameGenerator>(GAAudioFrameGenerator::Create(
       conf->audio_channels, conf->audio_samplerate));
  audioGenerator = generator.get();
  GlobalConfiguration::SetCustomizedAudioInputEnabled(true,
                                                      std::move(generator));
#endif
  P2PClientConfiguration config;

  std::string codec = ga_conf_readstr("video-codec");

  VideoCodecParameters video_param;
  if (ga_is_h265(codec)) {
    video_param.name = VideoCodec::kH265;
    ga_logger(Severity::INFO, "selected H265 codec\n");
  } else if (ga_is_av1(codec)) {
    video_param.name = VideoCodec::kAv1;
    ga_logger(Severity::INFO, "selected AV1 codec\n");
  } else {
    video_param.name = VideoCodec::kH264;
    ga_logger(Severity::INFO, "selected H264 codec\n");
  }
  VideoEncodingParameters video_encoding_param(video_param, 0, false);
  config.video_encodings.push_back(video_encoding_param);

  std::string coturn_ip = ga_conf_readstr("coturn-ip");
  if (!coturn_ip.empty())
  {
    IceServer stun_server, turn_server;

    ga_logger(Severity::INFO, "coturn_ip = %s\n", coturn_ip.c_str());
    std::string coturn_username = ga_conf_readstr("coturn-username");
    std::string coturn_password = ga_conf_readstr("coturn-password");
    std::string coturn_port = ga_conf_readstr("coturn-port");

    stun_server.urls.push_back("stun:" + coturn_ip + ":" + coturn_port);
    stun_server.username = coturn_username;
    stun_server.password = coturn_password;
    config.ice_servers.push_back(stun_server);

    turn_server.urls.push_back("turn:" + coturn_ip + ":" + coturn_port + "?transport=tcp");
    turn_server.urls.push_back("turn:" + coturn_ip + ":" + coturn_port + "?transport=udp");
    turn_server.username = std::move(coturn_username);
    turn_server.password = std::move(coturn_password);
    config.ice_servers.push_back(turn_server);
  }
  else {
    ga_logger(Severity::INFO, "*** no coturn server specified.\n");
  }

  p2pclient_.reset(new P2PClient(config, signaling));
  p2pclient_->AddObserver(*this);
  std::future<int32_t> connect_done = connect_status_.get_future();
  std::weak_ptr<ga::webrtc::ICSP2PClient> weak_this = shared_from_this();
  p2pclient_->AddAllowedRemoteId(client_peer_id);
  uint32_t client_clones = (uint32_t) ga_conf_readint("client-clones");
  for (uint32_t i = 1; i <= client_clones; i++) {
      p2pclient_->AddAllowedRemoteId(client_peer_id + "-clone" + std::to_string(i));
  }
  ga_logger(Severity::INFO, "Allow multi clone clients up to %lu\n", client_clones);

  p2pclient_->Connect(get_p2p_server(), server_peer_id,
                      [weak_this](const std::string& id) {
                        auto that = weak_this.lock();
                        if (!that)
                          return;
                        that->ConnectCallback(false, "");
                      },
                      [weak_this](std::unique_ptr<Exception> err) {
                        auto that = weak_this.lock();
                        if (!that)
                          return;
                        that->ConnectCallback(true, err->Message());
                      });
  RegisterCallbacks();
  CreateStream();

  dump_file_ = nullptr;

  if (enable_dump_) {
    char dumpFileName[128];
    snprintf(dumpFileName, 128, "gaVideoInput-%p.h264", this);
    dump_file_ = fopen(dumpFileName, "wb");
  }

  enable_render_drc_ = (ga_conf_readint("enable-render-drc") > 0)? true: false;

  return 0;
}

void ICSP2PClient::Deinit()
{
  if (stream_provider_)
    stream_provider_->DeRegisterEncoderObserver(*this);
  if (publication_)
    publication_->RemoveObserver(*this);
  if (p2pclient_) {
    p2pclient_->RemoveObserver(*this);
    p2pclient_->Stop(remote_user_id_, nullptr, nullptr);
    p2pclient_->Disconnect(nullptr, nullptr);
  }
  if (local_stream_)
    local_stream_->Close();
  if (local_audio_stream_)
    local_audio_stream_->Close();

#ifndef WIN32
  if (ga_conf_readbool("measure-latency", 0) == 1) {
    android::atrace_deinit();
  }
#endif
}

int32_t ICSP2PClient::Start() {
  if (encoder_register_client(this) < 0)
    return -1;
  return 0;
}

void ICSP2PClient::ConnectCallback(bool is_fail, const std::string &error) {
  if(!is_fail && (ga_conf_readbool("k8s", 0) == 1)) {
    std::string filePath = ga_conf_readstr("aic-workdir") + "/" + ".p2p_status";
    std::ofstream statusFile;
    statusFile.open(filePath);
    statusFile << "started" <<"\n";
    statusFile.close();
  }
  connect_status_.set_value(is_fail);
}

void ICSP2PClient::OnMessageReceived(const std::string &remote_user_id,
                                  const std::string message) {
  send_blocked_ = false;
  if (message == "start") {
    p2pclient_->Publish(remote_user_id, local_stream_,
      [&](std::shared_ptr<owt::p2p::Publication> pub) {
        streaming_ = true;
        ga_encoder_->RequestKeyFrame();
        RequestCursorShape();
        publication_ = std::move(pub);
        publication_->AddObserver(*this);
      },
      nullptr);
    bool clone_client = ga_conf_readint("client-clones") >= 1 && remote_user_id.find("-clone") != std::string::npos;
    if (!clone_client) {
      remote_user_id_ = remote_user_id;
    }

#ifdef WIN32
    audioGenerator->ClientConnectionStatus(true);
    if (hook_client_status_function_ == NULL) {
      hook_client_status_function_ = [](bool status) {
        if (status) {
          ga_logger(Severity::INFO, "hook-function: client connection message received.\n");
        } else {
          ga_logger(Severity::INFO, "hook-function: client disconnect message received.\n");
        }
      };
    }
    hook_client_status_function_(true);
#endif

    if (local_audio_stream_.get())
      p2pclient_->Publish(remote_user_id, local_audio_stream_, nullptr, nullptr);

    if (ga_conf_readbool("enable-multi-user", 0) != 0) {
      int32_t userId = ga_conf_readint("user");
      std::string str = "{\"key\":\"user-id\",\"val\":\"" + std::to_string(userId) + "\"}";
      p2pclient_->Send(remote_user_id, str.c_str(), nullptr, nullptr);
    }
  } else {
    // Set client event for round trip delay calculation feature
    nlohmann::json j1 = nlohmann::json::parse(message);
    if (j1["type"].is_string() &&
      (j1["type"].get<std::string>() == "control") &&
      (j1["data"].is_object()) &&
      (j1["data"]["event"].is_string())) {
      std::string event_type = j1["data"]["event"];
      if (event_type == "framestats") {
        //ga_logger(Severity::INFO, "Debugs: event type %s\n", event_type);
        if (j1["data"]["parameters"].is_object()) {
          nlohmann::json event_param = j1["data"]["parameters"];
#ifdef E2ELATENCY_TELEMETRY_ENABLED
          // E2Elatency via framestats
          if (event_param["E2ELatency"].is_number()) {
            HandleLatencyMessage(event_param["E2ELatency"]);
          }
#endif
          if (event_param.size() >= 5 &&
              event_param["framets"].is_number() &&
              event_param["framesize"].is_number() &&
              event_param["framedelay"].is_number() &&
              event_param["framestartdelay"].is_number() &&
              event_param["packetloss"].is_number()) {
            int32_t f_ts = event_param["framets"];
            int32_t f_size = event_param["framesize"];
            int32_t f_delay = event_param["framedelay"];
            int32_t f_start_delay = event_param["framestartdelay"];
            int32_t p_loss = event_param["packetloss"];

            ga_logger(Severity::DBG, "ics-p2p-client: OnMessageRecvd: f_ts=%ld, f_size=%ld, f_delay=%ld, f_start_delay=%ld, p_loss=%ld\n",
                f_ts, f_size, f_delay, f_start_delay, p_loss);

            if (ga_encoder_)
                ga_encoder_->SetFrameStats(f_ts, f_size, f_delay, f_start_delay, p_loss);

          }
        }
        return;
#ifndef WIN32
      } else if (event_type == "camerainfo") {
        ga_logger(Severity::INFO, "Received camera capability info from client\n");
        // Update camera_info to complete negotiation
        // protocol between client and Android vHAL.
        camera_client_handler_->updateCameraInfo(message);
        return;
      } else if (enable_render_drc_ && event_type == "sizechange") {
        if (j1["data"]["parameters"].is_object()) {
          nlohmann::json event_param = j1["data"]["parameters"];
          if (event_param["rendererSize"].is_object()) {
            if (event_param["rendererSize"].size()== 2 &&
                event_param["rendererSize"]["width"].is_number() &&
                event_param["rendererSize"]["height"].is_number()) {
              int32_t newWidth = event_param["rendererSize"]["width"];
              int32_t newHeight = event_param["rendererSize"]["height"];
              if (ga_encoder_)
                ga_encoder_->ChangeRenderResolution(newWidth, newHeight);
            }
          }
        }
        return;
      } else if (event_type == "videoalpha") {
        if (j1["data"]["parameters"].is_object()) {
          nlohmann::json event_param = j1["data"]["parameters"];
          if (event_param["action"].is_number()) {
            uint32_t action = event_param["action"];
            if (ga_encoder_) {
              ga_encoder_->SetVideoAlpha(action);
              const char* ack = "{\"key\":\"video-alpha-success\"}";
              p2pclient_->Send(remote_user_id_, ack, nullptr, nullptr);
            }
          }
        }
        return;
      } else if (event_type == "sensorcheck") {
        sensor_handler_->configureClientSensors();
        sensor_handler_->setClientRequestFlag(true);
        return;
      } else if (event_type == "sensordata") {
        sensor_handler_->processClientMsg(message);
        return;
      } else if (event_type == "gps") {
        nlohmann::json event_param = j1["data"]["parameters"];
        std::string data = event_param["data"];
        ssize_t sts = 0;
        std::string err;
        std::tie(sts, err) = virtual_gps_receiver_->Write((uint8_t *)data.c_str(), data.length());
        if (sts < 0)
          ga_logger(Severity::ERR, "Failed to write GPS data: %s\n", err.c_str());
        return;
      } else if (event_type == "cmdchannel") {
        command_channel_handler_->processClientMsg(message);
        return;
#ifdef E2ELATENCY_TELEMETRY_ENABLED
      } else if (event_type == "touch") {
        nlohmann::json event_param = j1["data"]["parameters"];
        if (event_param["E2ELatency"].is_number()) {
          HandleLatencyMessage(event_param["E2ELatency"]);
        }
#endif
#endif
      }
    }
    controller_->PushClientEvent(message);

#ifdef WIN32
    nlohmann::json j = nlohmann::json::parse(message);
    // Data validation.
    if (j["type"].is_string() &&
        (j["type"].get<std::string>() == "control") &&
        (j["data"].is_object()) &&
        (j["data"]["event"].is_string())) {
      std::string event_type = j["data"]["event"];
      if ((event_type == "mousemove") &&
          (j["data"]["parameters"].is_object())) {
        // Mouse motion.
        nlohmann::json event_param = j["data"]["parameters"];
        if (event_param.size() > 4 &&
            event_param["eventTimeSec"].is_number() &&
            event_param["eventTimeUsec"].is_number()) {
          struct timeval tv;
          tv.tv_sec = event_param["eventTimeSec"];
          tv.tv_usec = event_param["eventTimeUsec"];
          if (ga_encoder_)
            ga_encoder_->SetClientEvent(tv);
        }
      }
      else {
        if (event_type == "keydown" &&
          (j["data"]["parameters"].is_object())) {
          if (ga_encoder_) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ga_encoder_->SetClientEvent(tv);
          }
        }
      }
    }
#endif
  }
}

void ICSP2PClient::SendCursor(std::shared_ptr<CURSOR_DATA> cursor_data) {
  if (!streaming_ || send_blocked_)
    return;
#ifdef WIN32
  auto cursor_msg =
      ga::webrtc::CursorUtils::GetJsonForCursorInfo(*cursor_data.get());
  if(p2pclient_)
      p2pclient_->Send(remote_user_id_, cursor_msg, [this]() {
          send_failures_ = 0;
          send_blocked_ = false;
      },
      nullptr);
#endif
}

void ICSP2PClient::SendQoS(std::shared_ptr<QosInfo> qos_info) {
  if (!streaming_ || send_blocked_)
    return;
  auto qos_msg = ga::webrtc::QoSUtils::GetJsonForQoSInfo(*qos_info.get());
  p2pclient_->Send(remote_user_id_, qos_msg, [this]() {
        send_failures_ = 0;
        send_blocked_ = false;
      },
    [this](std::unique_ptr<owt::base::Exception>) {
      send_failures_++;
      if (send_failures_ >= OWT_MAX_SEND_FAILURES) {
        send_blocked_ = true;
      }
    });
}

void ICSP2PClient::CreateStream() {
  bool audio_enabled = ga_conf_readbool("enable-audio", 1);
  bool av_bundle = ga_conf_readbool("av-bundle", 1);
  ga_encoder_ = std::make_unique<GAVideoEncoder>();
  stream_provider_ = owt::base::EncodedStreamProvider::Create();
  stream_provider_->RegisterEncoderObserver(*this);
  std::shared_ptr<owt::base::LocalCustomizedStreamParameters> lcsp(new LocalCustomizedStreamParameters(av_bundle, true));
  lcsp->Resolution(640, 480);
  int32_t error_code = 0;
  local_stream_ = LocalStream::Create(std::move(lcsp), stream_provider_);
  if (audio_enabled && !av_bundle) {
    owt::base::LocalCameraStreamParameters lcspc(true, false);
    local_audio_stream_ = LocalStream::Create(lcspc, error_code);
  }
}

void ga::webrtc::ICSP2PClient::RequestCursorShape() {
  ga_module_t *video_encoder = encoder_get_vencoder();
  video_encoder->ioctl(GA_IOCTL_REQUEST_NEW_CURSOR, 0, nullptr);
}

void ICSP2PClient::InsertFrame(ga_packet_t* packet) {
  // Each time InsertFrame is invoked, we update the bandwidth to encoder wrapper.
  if (packet == nullptr || !capturer_started_) {
      return;
  }

  if (clock_ == nullptr)
    clock_.reset(new owt::base::Clock());
  if (g_bEnableOwtStats && p2pclient_ && streaming_) {
    p2pclient_->GetConnectionStats(remote_user_id_,
      [&](std::shared_ptr<owt::base::RTCStatsReport> report) {
        for (const auto& stat_rec : *report) {
          if (stat_rec.type == owt::base::RTCStatsType::kOutboundRTP) {
            auto& stat = stat_rec.cast_to<owt::base::RTCOutboundRTPStreamStats>();
            if (stat.kind == "video") {
              bytes_sent_on_last_stat_call_ = stat.bytes_sent;
            }
          } else if (stat_rec.type == owt::base::RTCStatsType::kCandidatePair) {
            auto& stat = stat_rec.cast_to<owt::base::RTCIceCandidatePairStats>();
            if (stat.nominated) {
              current_available_bandwidth_ = stat.available_outgoing_bitrate;
            }
          } else {
            // Ignore other stats.
          }
        }
      },
      [](std::unique_ptr<owt::base::Exception> err) {});
  }
  if (!capturer_started_ || !stream_provider_.get())
    return;

  owt::base::EncodedImageMetaData meta_data;
  int32_t side_data_len = sizeof(FrameMetaData);
  FrameMetaData* side_data = (FrameMetaData*)ga_packet_get_side_data(
      packet, ga_packet_side_data_type::GA_PACKET_DATA_NEW_EXTRADATA,
      &side_data_len);

  if (!side_data)
    return;

  if (packet->flags & GA_PKT_FLAG_KEY)
      meta_data.is_keyframe = true;

#ifndef DISABLE_TS_FT
  meta_data.picture_id = static_cast<uint16_t>(packet->pts);
#else
  meta_data.picture_id = 0;
#endif

  meta_data.last_fragment = (side_data->last_slice);
  meta_data.capture_timestamp = clock_->TimeInMilliseconds();

  meta_data.encoding_start = meta_data.capture_timestamp +
      side_data->encode_start_ms -
      side_data->capture_time_ms;
  meta_data.encoding_end = meta_data.capture_timestamp +
      side_data->encode_end_ms -
      side_data->capture_time_ms;

  ga_logger(Severity::DBG, "ics-p2p-client: packet->flags = %d\n", packet->flags);

#ifdef E2ELATENCY_TELEMETRY_ENABLED
  // E2ELatency
  meta_data.picture_id   = UpdateFrameNumber();
  uint32_t frame_to_send = GetFrameNumber();

  struct {
    uint64_t encode_time_ms = 0; // Time taken by server to encode the frame.
    uint64_t render_time_ms = 0; // Time taken by server to produce a frame with client input.
                                 // NOTE: we assume that the next frame encoded after receiving
                                 // client input does contain an update triggered by the client.
    uint64_t send_time_ms = 0;   // Time server sends latency message to server.
  } server_latency_;

  server_latency_.encode_time_ms = (uint64_t)(meta_data.encoding_end - meta_data.encoding_start);
  server_latency_.send_time_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  
  // There are 2 cases:
  //
  // 1. We are about to send the frame which contains update requested by client
  //                                                         server sends
  //                                                         frame
  //                                                         ^
  //                                                         |
  // ---------------------------[ frame encoding...  ]------------------------> time
  //    ^                    ^
  //    |                    |
  //  key pressed           key event received
  //  on client side        on server side
  //
  // 2. We are about to send the frame which we started to encode before we
  //    received client input and it does not contain update requested by client.
  //    For such a frame we send only encode_time_ms.
  //                                                         server sends
  //                                                         frame
  //                                                         ^
  //                                                         |
  // -------[ frame encoding...  ]---[ frame encoding...  ]-------------------> time
  //    ^                    ^
  //    |                    |
  //  key pressed           key event received
  //  on client side        on server side
  //
  // NOTE: E2E latency is not driven by real key presses.
  //       E2E latency is calculated using a dummy "key press" that occurs at most once per rendered frame.
  //       Actual frequency will be lower if E2E latency is greater than frame time.
  //

  //                                 (                 Amount of time we received client input before rendering ended                 );
  //                                 (                 Time rendering ended                          -    Time received client input  );
  //                                 (     Time encoding ended     -   Time taken to encode frame  ) - (  Time received client input  );
  int64_t render_client_input_time = (server_latency_.send_time_ms - server_latency_.encode_time_ms) - client_latency_.received_time_ms;
  if (render_client_input_time <= 0) {
    // The message was received after the beginning of encoding, should wait until next InsertFrame call
    frame_delay_++;
    ga_logger(Severity::DBG, "changing frame_delay to %lu\n", frame_delay_);
  } else {
    server_latency_.render_time_ms = (uint64_t)render_client_input_time;
  }

  bool send_e2e_latency_stats = HasClientStats() && (frame_to_send == client_latency_.received_frame_number + frame_delay_);
  
  nlohmann::json output_message;
  if (send_e2e_latency_stats) {
    output_message["clientSendLatencyTime"]       = client_latency_.send_time_ms;
    output_message["serverReceivedLatencyTime"]   = client_latency_.received_time_ms;
    output_message["serverRenderClientInputTime"] = server_latency_.render_time_ms;
  }
  output_message["serverEncodeFrameTime"]         = server_latency_.encode_time_ms;

  std::string latency_msg_string = output_message.dump();
  // copy message to meta data
  if (!latency_msg_string.empty()) {
    // allocate memory for latency message
    meta_data.encoded_image_sidedata_new(latency_msg_string.size());
    uint8_t* p_latency_message = meta_data.encoded_image_sidedata_get();
    size_t latency_message_size = meta_data.encoded_image_sidedata_size();
    // copy the message
    if (latency_msg_string.size() > 0 && p_latency_message) {
      memcpy(p_latency_message, latency_msg_string.data(), latency_msg_string.size());
    }
    ga_logger(Severity::DBG, "ics-p2p-client: InsertFrame: Frame delay is %lu, Frame %lu: msg_size %zu: Latency message sent from server: %s\n",
      frame_delay_, frame_to_send, latency_message_size, latency_msg_string.c_str());

    if (send_e2e_latency_stats) {
      client_latency_.send_time_ms          = 0;
      client_latency_.received_time_ms      = 0;
      client_latency_.received_frame_number = 0;

      frame_delay_ = 1;
    }
  }
  // End of E2ELatency
#endif

  std::vector<uint8_t> buffer;
  if (packet->data != nullptr && packet->size > 0) {
    buffer.insert(buffer.begin(), packet->data, packet->data + packet->size);
    if (dump_file_) {
      fwrite(packet->data, 1, packet->size, dump_file_);
    }
#ifndef WIN32
    if (android::is_atrace_enabled()) {
      static int32_t nS3Count = 0;
      nS3Count++;
      std::string str = "atou S3 ID: " + std::to_string(nS3Count) + " size: " + std::to_string(packet->size) + " " + remote_user_id_;
      android::atrace_begin(str);
      android::atrace_end();
    }
#endif
    stream_provider_->SendOneFrame(buffer, meta_data);

#ifdef E2ELATENCY_TELEMETRY_ENABLED
    // free memory for latency message
    meta_data.encoded_image_sidedata_free();
#endif
  }

  ga_encoder_->SetMaxBps(current_available_bandwidth_);
}

// Once this is called, we will reset the credit bytes.
int64_t ICSP2PClient::GetCreditBytes() {
  int64_t credit_bytes = bytes_sent_on_last_stat_call_ - bytes_sent_on_last_credit_call_;
  bytes_sent_on_last_credit_call_ = bytes_sent_on_last_stat_call_;
  return credit_bytes;
}

int64_t ICSP2PClient::GetMaxBitrate() {
  return current_available_bandwidth_;
}

void ICSP2PClient::OnStreamAdded(
    std::shared_ptr<owt::base::RemoteStream> stream) {
#ifndef WIN32
  ga_logger(Severity::INFO, "OnStreamAdded\n");
  remote_stream_handler_->setStream(std::move(stream));
  remote_stream_handler_->subscribeForAudio();
#endif
}

void ICSP2PClient::OnStarted() {
  capturer_started_ = true;
  if (ga_encoder_ != nullptr)
    ga_encoder_->RequestKeyFrame();
}

void ICSP2PClient::OnStopped() {
  capturer_started_ = false;
  streaming_ = false;
#ifndef WIN32
  sensor_handler_->setClientRequestFlag(false);
#endif
}

void ICSP2PClient::OnKeyFrameRequest() {
  if (ga_encoder_ != nullptr) {
    ga_encoder_->RequestKeyFrame();
  }
}

void ICSP2PClient::OnEnded() {
    ga_logger(Severity::INFO, "ended.");
}

void ICSP2PClient::OnPeerConnectionClosed(const std::string& remote_user_id) {
#ifdef WIN32
    if (hook_client_status_function_ == NULL) {
        hook_client_status_function_ = [](bool status) {
            if (status) {
                ga_logger(Severity::INFO, "hook-function: client connection message received.\n");
            } else {
                ga_logger(Severity::INFO, "hook-function: client disconnect message received.\n");
            }
        };
    }
    hook_client_status_function_(false);
#endif

    uint32_t client_clones = (uint32_t) ga_conf_readint("client-clones");
    if (client_clones >= 1 && remote_user_id.find("-clone") != std::string::npos) {
        ga_logger(Severity::INFO, "Do nothing for clone client stop\n");
        return;
    }

    ga_logger(Severity::INFO, "on_stopped\n");
    if (ga_encoder_ != nullptr) {
      ga_encoder_->Pause();
    }
#ifdef WIN32
    audioGenerator->ClientConnectionStatus(false);
#endif
}

void ICSP2PClient::OnRateUpdate(uint64_t bitrate_bps, uint32_t frame_rate) {
    // Do nothing here.
}
