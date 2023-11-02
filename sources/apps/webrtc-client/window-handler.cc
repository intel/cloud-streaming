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
#include "ga-option.h"
#include "window-class.h"
#include "control-handler.h"
#include "window-handler.h"
// clang-format on

WindowHandler *WindowHandler::window_handler_ = nullptr;

WindowHandler *WindowHandler::GetInstance(void) {
  if (!window_handler_) {
    window_handler_ = new WindowHandler();
  }
  return window_handler_;
}

void WindowHandler::OnMouseStateChange(struct ga::remote::CursorInfo &cursorInfo) {
  HWND hwnd;

  hwnd = WindowHandler::GetInstance()->wc_->hwnd_;
  if (!cursorInfo.isVisible) {
    SendMessage(hwnd, WM_GA_CURSOR_VISIBLE, 0, GA_HIDE_CURSOR);
  } else {
    SendMessage(hwnd, WM_GA_CURSOR_VISIBLE, 0, GA_SHOW_CURSOR);
  }
}

int WindowHandler::OnGameServerConnected(std::string &session_id) {
  if (!session_id.empty()) {
    WindowHandler* window_handler = WindowHandler::GetInstance();
    window_handler->session_id_ = session_id;
    window_handler->connected_ = true;
  }
  return 0;
}

int WindowHandler::InitializeGameWindow(HINSTANCE h_instance, int n_cmd_show,
                                       const char *window_title) {
  int ret_value = 0;
  wc_.reset(
      new WindowClass(h_instance, n_cmd_show, window_title));
  return ret_value;
}

HWND WindowHandler::GetWindowHandle(void) {
  HWND h_wnd = nullptr;
  if (wc_) {
    h_wnd = wc_->hwnd_;
  }
  return h_wnd;
}

void WindowHandler::GetWindowSize(int& width, int& height) {
  width = wc_->client_window_width_;
  height = wc_->client_window_height_;
}

void WindowHandler::Destroy(void) {
  if (!session_id_.empty()) {
    ga::remote::ExitGame(session_id_);
  }
  if (wc_) {
    wc_->Destroy();
  }
  if (window_handler_) {
    delete window_handler_;
    window_handler_ = nullptr;
  }
}

