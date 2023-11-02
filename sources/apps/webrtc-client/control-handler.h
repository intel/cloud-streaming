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

#ifndef GA_CONTROL_HANDLER_H_
#define GA_CONTROL_HANDLER_H_

#include <map>
#include <string>
#include <windows.h>

#define GA_LEGACY_INPUT 1
#define GA_RAW_INPUT 2

enum MouseEvent {
  kMouseMove = 0,
  kMouseLeftButton = 1,
  kMouseMiddleButton = 2,
  kMouseRightButton = 3,
  kMouseWheel = 4
};

enum MouseButtonState {
  kMouseButtonUp = 1,
  kMouseButtonDown = 2 
};

struct KeyboardOptions {
  WPARAM v_key_;
  UINT msg_;
};

struct MouseOptions {
  int x_pos_;
  int y_pos_;
  int delta_x_;
  int delta_y_;
  int delta_z_;
  int is_cursor_relative_;
  MouseEvent m_event_;
  MouseButtonState m_button_state_;
};

struct FrameStats {
  long ts = 0;
  long size;
  int  delay;
  long start_delay = 0;
  long p_loss;
  UINT64 latencymsg_;
};

class InputEventHandler {
public:
  static std::string OnKeyboardEvent(KeyboardOptions *p_k_options);
  static std::string OnMouseEvent(MouseOptions *p_m_options, bool is_raw);
  static std::string OnSizeChange(UINT render_w, UINT render_h);
  static std::string onPointerlockchange(bool relativeMode);
  static std::string OnStatsRequest(FrameStats* p_framestats);
};

#endif // GA_CONTROL_HANDLER_H_

