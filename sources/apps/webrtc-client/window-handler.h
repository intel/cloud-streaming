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

#ifndef GA_WINDOW_HANDLER_H_
#define GA_WINDOW_HANDLER_H_

// clang-format off
#include <string>
#include <thread>
#include <windows.h>
#include "window-class.h"
#include "ga-option.h"
// clang-format on

class WindowHandler {
public:
  static void OnMouseStateChange(struct ga::remote::CursorInfo &cursorInfo);
  static int OnGameServerConnected(std::string &session_id);
  static WindowHandler *GetInstance(void);
  void GetWindowSize(int& width, int& height);
  int InitializeGameWindow(HINSTANCE h_instance, int n_cmd_show, const char *window_title);
  HWND GetWindowHandle();
  void Destroy();
private:
  static WindowHandler* window_handler_;
  bool connected_;
  std::unique_ptr<WindowClass> wc_;
  std::string session_id_;

  WindowHandler() {}
  ~WindowHandler() {}
};

#endif // GA_WINDOW_HANDLER_H_

