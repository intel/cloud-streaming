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
#include "window-class.h"
#include "ga-option.h"
#include "Shellscalingapi.h"
// clang format on

#pragma comment(lib, "Shcore.lib")

#define WM_MOUSE_MOVE_SHIFT_H 16

float screen_scale_factor_ = 1.0f;

void WindowClass::Destroy(void) {
  if (this->hwnd_) {
    DestroyWindow(this->hwnd_);
  }
  delete this;
}

WindowClass::WindowClass(HINSTANCE h_instance, int n_cmd_show, const char* window_title) {
  LONG l_ex_window_style;
  RECT client_rect;
  int width_delta = 0; 
  int height_delta = 0;

  wc_.cbSize = sizeof(WNDCLASSEX);
  wc_.style = 0;
  wc_.lpfnWndProc = WindowClass::PreInitWndProc;
  wc_.cbClsExtra = 0;
  wc_.cbWndExtra = 0;
  wc_.hInstance = h_instance;
  wc_.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc_.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc_.hbrBackground = (HBRUSH)(CTLCOLOR_EDIT + 1);
  wc_.lpszMenuName = NULL;
  wc_.lpszClassName = L"GaWebRTCClient";
  wc_.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

  if (!RegisterClassEx(&wc_)) {
    MessageBox(NULL, L"Window Registration Failed!", L"Error!",
               MB_ICONEXCLAMATION | MB_OK);
  }

  initial_window_height_ = GetSystemMetrics(SM_CYSCREEN);
  initial_window_width_ = GetSystemMetrics(SM_CXSCREEN);
  client_window_height_ = initial_window_height_;
  client_window_width_ = initial_window_width_;
  scale_ratio_w_ = 1.0;
  scale_ratio_h_ = 1.0;

  hwnd_ = CreateWindowEx(WS_EX_APPWINDOW,
                          L"GaWebRTCClient", L"GameWindow",
                          WS_POPUP,
                          CW_USEDEFAULT, CW_USEDEFAULT, client_window_width_, client_window_height_, NULL,
                          NULL, h_instance, this);
  
  if (hwnd_ == NULL) {
      MessageBox(NULL, L"CreateWindowEx Failed!", L"Error!",
          MB_ICONEXCLAMATION | MB_OK);
      exit(-1);
  }

  l_ex_window_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
  l_ex_window_style &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
  SetWindowLong(hwnd_, GWL_EXSTYLE, l_ex_window_style);

  GetWindowRect(hwnd_, &window_rect_);
  GetClientRect(hwnd_, &client_rect);

  if ((client_rect.right - client_rect.left) != initial_window_width_) {
    width_delta = initial_window_width_ - (client_rect.right - client_rect.left);
  }
  if ((client_rect.bottom - client_rect.top) != initial_window_height_) {
    height_delta = initial_window_height_ - (client_rect.bottom - client_rect.top);
  }

  window_rect_.right += width_delta-2* GetSystemMetrics(SM_CXEDGE);
  window_rect_.bottom += height_delta-2* GetSystemMetrics(SM_CYEDGE);

  SetWindowPos(hwnd_, 0, window_rect_.left, window_rect_.top,
               GetSystemMetrics(SM_CXSCREEN),
               GetSystemMetrics(SM_CYSCREEN),
               SWP_NOZORDER | SWP_NOACTIVATE);

  GetClientRect(hwnd_, &client_rect);
  GetWindowRect(hwnd_, &window_rect_);

  ShowWindow(hwnd_, n_cmd_show);
  UpdateWindow(hwnd_);
  game_mode_toggle_ = true;
  full_screen_toggle_ = false;
  in_sys_key_down_ = false;

  HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
  if (monitor != NULL) {
      MONITORINFOEX miex;
      miex.cbSize = sizeof(miex);
      GetMonitorInfo(monitor, &miex);
      int cxLogical = (miex.rcMonitor.right - miex.rcMonitor.left);
      int cyLogical = (miex.rcMonitor.bottom - miex.rcMonitor.top);

      // Get the physical width and height of the monitor.
      DEVMODE dm;
      dm.dmSize = sizeof(dm);
      dm.dmDriverExtra = 0;
      EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &dm);
      int cxPhysical = dm.dmPelsWidth;
      int cyPhysical = dm.dmPelsHeight;

      // Calculate the scaling factor.
      double horzScale = ((double)cxPhysical / (double)cxLogical);
      double vertScale = ((double)cyPhysical / (double)cyLogical);

      screen_scale_factor_ = horzScale;
  }

  this->RegisterRawInput(hwnd_);
}

LRESULT CALLBACK WindowClass::PreInitWndProc(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param){
    LRESULT ret_val;
  if( msg == WM_NCCREATE ) {
    CREATESTRUCTW* p_create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
    WindowClass* const p_window_class = static_cast<WindowClass*>(p_create_struct->lpCreateParams);
    SetWindowLongPtr( h_wnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WindowClass::PostInitWndProc) );
    SetWindowLongPtr( h_wnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p_window_class) );
    ret_val = p_window_class->InternalWndProc( h_wnd,msg,w_param,l_param );
  } else {
    ret_val = DefWindowProc( h_wnd,msg,w_param,l_param);
  }
  return ret_val;
}

LRESULT CALLBACK WindowClass::PostInitWndProc(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  WindowClass* const p_window_class = reinterpret_cast<WindowClass*>(GetWindowLongPtr(h_wnd,GWLP_USERDATA));
  return p_window_class->InternalWndProc(h_wnd,msg,w_param,l_param);
}

LRESULT CALLBACK WindowClass::InternalWndProc(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  LPARAM mouse_x;
  LPARAM mouse_y;
  LPARAM scaled_lparam;
  LRESULT result = 0;

  switch (msg) {
  case WM_MOUSEMOVE:
  case WM_LBUTTONUP:
  case WM_LBUTTONDOWN:
  case WM_MBUTTONUP:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONUP:
  case WM_RBUTTONDOWN:
    mouse_x = (float)GET_X_LPARAM(l_param);
    mouse_x -= x_render_offset_;

    mouse_y = (float)GET_Y_LPARAM(l_param);
    mouse_y -= y_render_offset_;
    scaled_lparam = (LPARAM)(ceil(this->scale_ratio_h_ * mouse_y));
    scaled_lparam <<= WM_MOUSE_MOVE_SHIFT_H;
    scaled_lparam = scaled_lparam | (LPARAM)(ceil(this->scale_ratio_w_ * mouse_x));


    ga::remote::SendInput(msg, w_param, scaled_lparam);
    break;
  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_INPUT:
    ga::remote::SendInput(msg, w_param, l_param);
    break;
  case WM_SYSKEYDOWN:
    if ((w_param == VK_UP) && (HIWORD(l_param) & KF_ALTDOWN) &&
        !this->in_sys_key_down_) {
      this->in_sys_key_down_ = true;
      if (this->full_screen_toggle_) {
        this->ChangeWindowedMode(h_wnd, false);
        this->full_screen_toggle_ = false;
        this->ChangeGameMode(NULL, false);
      } else {
        this->ChangeWindowedMode(h_wnd,true);
        this->full_screen_toggle_ = true;
        this->ChangeGameMode(NULL, false);
        this->ChangeGameMode(h_wnd, true);
      }
    }
    else if ((w_param == VK_OEM_PLUS) && (HIWORD(l_param) & KF_ALTDOWN) && !this->in_sys_key_down_) {
      this->in_sys_key_down_ = true;
      if (this->game_mode_toggle_) {
        this->ChangeGameMode(h_wnd, true);
        this->game_mode_toggle_ = false;
      }
      else{
       this->ChangeGameMode(NULL, false);
       this->game_mode_toggle_ = true;
      }
    }
    break;
  
  case WM_SYSKEYUP:
    this->in_sys_key_down_ = false;
    break;
  case WM_CLOSE:
    this->Destroy();
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_GA_CURSOR_VISIBLE:
    int disp_count;
    if (l_param == GA_SHOW_CURSOR) {
      disp_count = ShowCursor(TRUE);
      while (disp_count <= 0) {
        disp_count = ShowCursor(TRUE);
      }
      this->ChangeGameMode(h_wnd, false);
    }
    else {
      disp_count = ShowCursor(FALSE);
      while (disp_count >= 0) {
        disp_count = ShowCursor(FALSE);
      }
      if (!this->game_mode_toggle_) {
        this->ChangeGameMode(h_wnd, true);
      }
    }
  default:
    result = DefWindowProc(h_wnd, msg, w_param, l_param);
  }
  return result;
}

void WindowClass::ChangeWindowedMode(HWND hwnd, bool enable_fullscreen) {
  if (enable_fullscreen) {
    GetWindowRect(hwnd, &this->window_rect_);

    UINT w = GetSystemMetrics(SM_CXSCREEN);
    UINT h = GetSystemMetrics(SM_CYSCREEN);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, w, h, SWP_FRAMECHANGED);
    RECT rect;
    GetClientRect(hwnd, &rect);
    this->client_window_width_ = rect.right - rect.left;
    this->client_window_height_ = rect.bottom - rect.top;
  }
  else{
    UINT w;
    UINT h;

    this->client_window_width_ = this->initial_window_width_;
    this->client_window_height_ = this->initial_window_height_;
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW & ~(WS_SIZEBOX | WS_MAXIMIZEBOX));
    w = this->window_rect_.right - this->window_rect_.left;
    h = this->window_rect_.bottom - this->window_rect_.top;
    SetWindowPos(hwnd, NULL, this->window_rect_.left, this->window_rect_.top, w, h,
                     SWP_FRAMECHANGED);
  }
  int new_height = (this->client_window_width_ * this->initial_window_height_) / this->initial_window_width_;

  this->scale_ratio_w_ = (float)(this->initial_window_width_) / (float)(this->client_window_width_);
  this->scale_ratio_h_ = (float)(this->initial_window_height_) / (float)(new_height);
  x_render_offset_ = 0;
  y_render_offset_ = (this->client_window_height_ - new_height)/2;
  ga::remote::SetWindowSize(x_render_offset_, y_render_offset_, this->client_window_width_, new_height);
}

void WindowClass::ChangeGameMode(HWND hwnd, bool enable) {
  RECT window_rect;
  if (enable) { // true = releative mode.
    GetWindowRect(hwnd, &window_rect);
    ClipCursor(&window_rect);
    SetCapture(hwnd);
  }
  else{
    ClipCursor(NULL);
    ReleaseCapture();
  }
}

void WindowClass::RegisterRawInput(HWND hwnd) {
  RAWINPUTDEVICE Rid[1] = {0};

  Rid[0].usUsagePage = 0x01;
  Rid[0].usUsage = 0x02;
  Rid[0].dwFlags = 0;
  Rid[0].hwndTarget = hwnd;

  if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
  }
}

void WindowClass::UnregisterRawInput(void) {
  RAWINPUTDEVICE Rid[1] = {0};

  Rid[0].usUsagePage = 0x01;
  Rid[0].usUsage = 0x02;
  Rid[0].dwFlags = RIDEV_REMOVE;
  Rid[0].hwndTarget = NULL;

  if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
  }
}

