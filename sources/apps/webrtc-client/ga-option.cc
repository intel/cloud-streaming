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
#include "game-session.h"
#include <windows.h>
#include "control-handler.h"
// clang-format on

// utility helper functions
void PopulateCommonMouseOptionsLegacy(MouseOptions *ptr_m_options,
  LPARAM l_param);
bool PopulateCommonMouseOptionsRaw(MouseOptions *ptr_m_options,
  RAWINPUT *p_raw_input);
DWORD GetInputTypeFromRawInput(LPARAM l_param);


// globals for getting raw input
static bool g_bCursorRelativemode = false;
static const int kRawInputSize = 1024;
static char g_rawinput[kRawInputSize];

std::unique_ptr<GameSession> g_remote_connection;

void ga::remote::ChangeCursorReportMode(bool RelativeMode)
{
  g_bCursorRelativemode = RelativeMode;
  g_remote_connection->SendPointerlockchange(RelativeMode);
}

int ga::remote::StartGame(const ga::remote::SessionMetaData& session_opts, const ga::remote::ClientSettings client_opts, StreamingStatistics* streamingStatistics) {
  int ret_code = 0;
  g_remote_connection.reset(new GameSession);
  g_remote_connection->SendSizeChange(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
  g_remote_connection->ConfigConnection(session_opts, client_opts);
  g_remote_connection->ConnectPeerServer(streamingStatistics);
  //TODO: need to add logic to see whether webrtc connect success or not.
  // Assume it is success, try to send SizeChange request to server to report current render resolution.
  return ret_code;
}

void ga::remote::SendInput(UINT input_message, WPARAM w_param,
  LPARAM l_param) {
  std::string m;
  KeyboardOptions key_options;
  MouseOptions m_options;
  DWORD raw_input_type;
  RAWINPUT *p_raw;

  switch (input_message) {
  case WM_KEYDOWN:
  case WM_KEYUP:
    key_options.msg_ = input_message;
    key_options.v_key_ = w_param;
    g_remote_connection->SendKeyboardEvent(&key_options);
    break;

  case WM_INPUT:
    if (g_bCursorRelativemode) {
      raw_input_type = GetInputTypeFromRawInput(l_param);
      if (raw_input_type == (DWORD)RIM_TYPEMOUSE) {
        p_raw = (RAWINPUT *)g_rawinput;
        if (PopulateCommonMouseOptionsRaw(&m_options, p_raw)) {
          g_remote_connection->SendMouseEvent(&m_options, true);
        }
      }
      else if (raw_input_type == (DWORD)RIM_TYPEKEYBOARD) {
        p_raw = (RAWINPUT *)g_rawinput;
        key_options.msg_ = p_raw->data.keyboard.Message;
        key_options.v_key_ = p_raw->data.keyboard.VKey;
        g_remote_connection->SendKeyboardEvent(&key_options);
      }
    }
    break;
  case WM_MOUSEMOVE:
   if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseMove;
      g_remote_connection->SendMouseEvent(&m_options, false);
   }
    break;
  case WM_LBUTTONUP:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseLeftButton;
      m_options.m_button_state_ = kMouseButtonUp;
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_LBUTTONDOWN:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseLeftButton;
      m_options.m_button_state_ = kMouseButtonDown;
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_MBUTTONUP:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseMiddleButton;
      m_options.m_button_state_ = kMouseButtonUp;

      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_MBUTTONDOWN:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseMiddleButton;
      m_options.m_button_state_ = kMouseButtonDown;
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_RBUTTONUP:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseRightButton;
      m_options.m_button_state_ = kMouseButtonUp;
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_RBUTTONDOWN:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseRightButton;
      m_options.m_button_state_ = kMouseButtonDown;
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  case WM_MOUSEWHEEL:
    if (!g_bCursorRelativemode) {
      PopulateCommonMouseOptionsLegacy(&m_options, l_param);
      m_options.m_event_ = kMouseWheel;
      m_options.delta_y_ = GET_WHEEL_DELTA_WPARAM(w_param);
      g_remote_connection->SendMouseEvent(&m_options, false);
    }
    break;
  default:
    break;
  }
}

void PopulateCommonMouseOptionsLegacy(MouseOptions *ptr_m_options,
  LPARAM l_param) {
  ptr_m_options->x_pos_ = l_param & 0xFFFF;
  ptr_m_options->y_pos_ = (l_param >> 16) & 0xFFFF;
  ptr_m_options->is_cursor_relative_ = 0;
}

DWORD GetInputTypeFromRawInput(LPARAM l_param) {
  UINT dwSize;
  RAWINPUT *raw;

  GetRawInputData((HRAWINPUT)l_param, RID_INPUT, NULL, &dwSize,
    sizeof(RAWINPUTHEADER));
  if (GetRawInputData((HRAWINPUT)l_param, RID_INPUT, g_rawinput, &dwSize,
    sizeof(RAWINPUTHEADER)) != dwSize) {
  }

  raw = (RAWINPUT *)g_rawinput;
  return (raw->header.dwType);
}

bool PopulateCommonMouseOptionsRaw(MouseOptions *ptr_m_options,
  RAWINPUT *p_raw_input) {

  bool ret_value = TRUE;
  ptr_m_options->is_cursor_relative_ = 1;
  ptr_m_options->x_pos_ = p_raw_input->data.mouse.lLastX;
  ptr_m_options->y_pos_ = p_raw_input->data.mouse.lLastY;
  switch (p_raw_input->data.mouse.usButtonFlags) {
  case RI_MOUSE_LEFT_BUTTON_DOWN:
    ptr_m_options->m_event_ = kMouseLeftButton;
    ptr_m_options->m_button_state_ = kMouseButtonDown;
    break;
  case RI_MOUSE_LEFT_BUTTON_UP:
    ptr_m_options->m_event_ = kMouseLeftButton;
    ptr_m_options->m_button_state_ = kMouseButtonUp;
    break;
  case RI_MOUSE_MIDDLE_BUTTON_DOWN:
    ptr_m_options->m_event_ = kMouseMiddleButton;
    ptr_m_options->m_button_state_ = kMouseButtonDown;
    break;
  case RI_MOUSE_MIDDLE_BUTTON_UP:
    ptr_m_options->m_event_ = kMouseMiddleButton;
    ptr_m_options->m_button_state_ = kMouseButtonUp;
    break;
  case RI_MOUSE_RIGHT_BUTTON_DOWN:
    ptr_m_options->m_event_ = kMouseRightButton;
    ptr_m_options->m_button_state_ = kMouseButtonDown;
    break;
  case RI_MOUSE_RIGHT_BUTTON_UP:
    ptr_m_options->m_event_ = kMouseRightButton;
    ptr_m_options->m_button_state_ = kMouseButtonUp;
    break;
  case RI_MOUSE_BUTTON_4_DOWN:
  case RI_MOUSE_BUTTON_4_UP:
  case RI_MOUSE_BUTTON_5_DOWN:
  case RI_MOUSE_BUTTON_5_UP:
    ret_value = FALSE;
    break;
  case RI_MOUSE_WHEEL:
    ptr_m_options->m_event_ = kMouseWheel;
    ptr_m_options->delta_y_ = p_raw_input->data.mouse.usButtonData;
    break;
  default:
    if (ptr_m_options->x_pos_ != 0 || ptr_m_options->y_pos_ != 0) {
      ptr_m_options->m_event_ = kMouseMove;
    }
    else {
      ret_value = FALSE;
    }
    break;
  }
  return ret_value;
}


int ga::remote::ExitGame(const string &session_id) {
  int err_code = 0;
  err_code = g_remote_connection->StopConnection();
  return err_code;
}

void ga::remote::SetWindowSize(UINT x_offset, UINT y_offset, UINT width,
  UINT height) {
  g_remote_connection->SetWindowSize(x_offset, y_offset, width, height);
}

FILE *ga::log::OpenFile(std::string fileName, std::string fileType)
{
    FILE* file = NULL;
    errno_t file_error = 0;

    std::string fullFileName = "C:\\Temp\\" + fileName + "_" + std::to_string(GetCurrentThreadId()) + "." + fileType;

    file_error = fopen_s(&file, fullFileName.c_str(), "a+");
    if (file_error) {
        perror("Client: OpenFile: Error opening log file.");
        return NULL;
    }

    return file;
}

void ga::log::WriteToMsg(std::string &logMsg, const char *format_string, ...)
{
    if (format_string == NULL) {
        return;
    }

    // Construct the formatted source message
    char buffer[MAX_LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format_string);
    vsnprintf(buffer, MAX_LOG_BUFFER_SIZE, format_string, args);
    va_end(args);

    logMsg += std::string(buffer);
}

bool ga::log::FlushMsgToFile(FILE *destFile, std::string &logMsg)
{
    if (destFile != NULL) {
        fprintf(destFile, "%s", logMsg.c_str());
        logMsg = "";
        return true;
    }

    logMsg = "";
    return false;
}

void ga::log::CloseFile(FILE *file)
{
    if (file != NULL) {
        fclose(file);
    }
}

bool ga::log::WriteToFile(std::string fileName, std::string fileType, const char *format_string, ...)
{
    FILE *file = ga::log::OpenFile(fileName, fileType);
    if (file == NULL) {
        return false;
    }

    std::string msg = "";

    if (format_string == NULL) {
        return false;
    }

    // Construct the formatted source message
    char buffer[MAX_LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format_string);
    vsnprintf(buffer, MAX_LOG_BUFFER_SIZE, format_string, args);
    va_end(args);

    msg += std::string(buffer);
    if (ga::log::FlushMsgToFile(file, msg) == false) {
        return false;
    }

    ga::log::CloseFile(file);
    return true;
}

bool ga::json::ParseMessage(rapidjson::Document& document, const std::string& message)
{
    document.Parse(message.c_str());
    if (document.HasParseError() || document.IsNull()) {
        return false;
    }
    return true;
}

rapidjson::Type ga::json::MemberType(const rapidjson::Document& document, const std::string& message)
{
    if (document.IsNull()) {
        return rapidjson::Type::kNullType;
    }
    if (!document.HasMember(message.c_str())) {
        return rapidjson::Type::kNullType;
    }
    return document[message.c_str()].GetType();
}

uint64_t ga::json::FromUint64(const rapidjson::Document& document, const std::string& message)
{
    if (ga::json::MemberType(document, message) != rapidjson::Type::kNumberType) {
        return (uint64_t) 0;
    }
    return document[message.c_str()].GetUint64();
}

std::string ga::json::FromString(const rapidjson::Document& document, const std::string& message)
{
    if (ga::json::MemberType(document, message) != rapidjson::Type::kStringType) {
        return "";
    }
    return document[message.c_str()].GetString();
}

bool ga::json::FromBool(const rapidjson::Document& document, const std::string& message)
{
    rapidjson::Type type = ga::json::MemberType(document, message);
    if (type != rapidjson::Type::kTrueType && type != rapidjson::Type::kFalseType) {
        return false;
    }
    return document[message.c_str()].GetBool();
}
