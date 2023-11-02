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

#include "rtc-signaling.h"

#include <algorithm>
#include <map>

P2PSignalingChannel::P2PSignalingChannel() : io_(new sio::client()) {}

void P2PSignalingChannel::AddObserver(P2PSignalingChannelObserver &observer) {
  observers_.push_back(&observer);
}

void P2PSignalingChannel::RemoveObserver(
    P2PSignalingChannelObserver &observer) {
  observers_.erase(remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}

// If the token is not url-encoded, there will be connection issue.
// TODO: protect the token with URL encoding/decoding.
void P2PSignalingChannel::Connect(
    const std::string &host, const std::string &token,
    std::function<void(const std::string &)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  connect_success_callback_ = on_success;
  std::map<std::string, std::string> query;
  query.insert(std::pair<std::string, std::string>("clientVersion", "4.2"));
  query.insert(std::pair<std::string, std::string>("clientType", "cpp"));
  query.insert(std::pair<std::string, std::string>(
      "token", token));
  io_->set_open_listener(std::bind(&connection_listener::on_connected,
                                   connection_listener_.get()));
  sio::socket::ptr socket = io_->socket();
  std::string ics_chat = "owt-message";
  std::string auth_done = "server-authenticated";
  socket->on(
      ics_chat,
      sio::socket::event_listener_aux(
          [&](std::string const &name, sio::message::ptr const &data,
              bool has_ack, sio::message::list &ack_resp) {
            if (data->get_flag() == sio::message::flag_object) {
              std::string msg = data->get_map()["data"]->get_string().data();
              std::string from = data->get_map()["from"]->get_string().data();
              for (auto it = observers_.begin(); it != observers_.end(); ++it)
                (*it)->OnSignalingMessage(msg, from);
            }
          }));
  socket->on(auth_done,
             sio::socket::event_listener_aux(
                 [&](std::string const &name, sio::message::ptr const &data,
                     bool has_ack, sio::message::list &ack_resp) {
                   if (data->get_flag() == sio::message::flag_object &&
                       connect_success_callback_) {
                     std::string msg =
                         data->get_map()["uid"]->get_string().data();
                     connect_success_callback_(msg);
                   }
                 }));
  io_->connect(host, query);
}

void P2PSignalingChannel::Disconnect(
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {}

void P2PSignalingChannel::SendMessage(
    const std::string &message, const std::string &target_id,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr jsonObject = sio::object_message::create();
  jsonObject->get_map()["to"] = sio::string_message::create(target_id);
  jsonObject->get_map()["data"] = sio::string_message::create(message);
  io_->socket()->emit("owt-message", jsonObject, [=](const sio::message::list& msg) {
    if (msg.size() > 0 && msg[0]->get_flag() == sio::message::flag_integer) {
      int64_t error_code = msg[0]->get_int();
      if (on_failure) {
        std::unique_ptr<owt::base::Exception> exception(
          new owt::base::Exception(ExceptionType::kP2PMessageTargetUnreachable,
            "Remote user cannot be reached."));
        on_failure(std::move(exception));
      }
      return;
    }

    if (on_success) {
      on_success();
    }
    });
}
