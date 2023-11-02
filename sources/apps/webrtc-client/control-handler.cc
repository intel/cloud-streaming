// Copyright (C) 2020-2023 Intel Corporation
//
// SPDX-License-Identifier: Apache-2

// clang-format off
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "control-handler.h"
#include "window-handler.h"
// clang-format on

extern float screen_scale_factor_;

std::string InputEventHandler::OnKeyboardEvent(KeyboardOptions *p_key_options) {
  WPARAM wparam = p_key_options->v_key_;
  UINT msg = p_key_options->msg_;

  rapidjson::Document event;
  event.SetObject();
  rapidjson::Document::AllocatorType &alloc = event.GetAllocator();

  rapidjson::Value parameters(rapidjson::kObjectType);
  parameters.SetObject();
  parameters.AddMember("which", wparam, alloc);

  rapidjson::Value data(rapidjson::kObjectType);
  switch (msg) {
    case WM_KEYDOWN:
      data.AddMember("event", "keydown", alloc);
      break;
    case WM_KEYUP:
      data.AddMember("event", "keyup", alloc);
      break;
    default:
      break;
  }
  data.AddMember("parameters", parameters, alloc);
  event.AddMember("type", "control", alloc);
  event.AddMember("data", data, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  event.Accept(writer);
  return buffer.GetString();
}

std::string InputEventHandler::OnMouseEvent(MouseOptions *p_m_options,
                                           bool is_raw) {
  rapidjson::Document event;
  event.SetObject();
  rapidjson::Document::AllocatorType& alloc = event.GetAllocator();

  float screen_scale_factor = screen_scale_factor_ >= 1.0f ? screen_scale_factor_ : 1.0f;
  
  int client_window_width, client_window_height;
  WindowHandler::GetInstance()->GetWindowSize(client_window_width, client_window_height);
  int x = (int) (((float) p_m_options->x_pos_ * screen_scale_factor_ / client_window_width) * 32767);
  int y = (int) (((float) p_m_options->y_pos_ * screen_scale_factor_ / client_window_height) * 32767);

  rapidjson::Value parameters(rapidjson::kObjectType);
  parameters.SetObject();

  parameters.AddMember("x", x, alloc);
  parameters.AddMember("y", y, alloc);
  parameters.AddMember("movementX", x, alloc);
  parameters.AddMember("movementY", y, alloc);

  rapidjson::Value data(rapidjson::kObjectType);
  switch (p_m_options->m_event_) {
  case kMouseMove:
    data.AddMember("event", "mousemove", alloc);
    break;
  case kMouseLeftButton:
    if (p_m_options->m_button_state_ == kMouseButtonDown) {
      data.AddMember("event", "mousedown", alloc);
      parameters.AddMember("which", 1, alloc);
    } else {
      data.AddMember("event", "mouseup", alloc);
      parameters.AddMember("which", 1, alloc);
    }
    break;
  case kMouseMiddleButton:
    if (p_m_options->m_button_state_ == kMouseButtonDown) {
      data.AddMember("event", "mousedown", alloc);
      parameters.AddMember("which", 2, alloc);
    } else {
      data.AddMember("event", "mouseup", alloc);
      parameters.AddMember("which", 2, alloc);
    }
    break;
  case kMouseRightButton:
    if (p_m_options->m_button_state_ == kMouseButtonDown) {
      data.AddMember("event", "mousedown", alloc);
      parameters.AddMember("which", 3, alloc);
    } else {
      data.AddMember("event", "mouseup", alloc);
      parameters.AddMember("which", 3, alloc);
    }
    break;
  case kMouseWheel:
    data.AddMember("event", "wheel", alloc);
  // Javascript client allows deltaX and deltaZ to be set.. so we have
  // to set them to 0, otherwise ga server will crash.
  parameters.AddMember("deltaX", 0, alloc);
    parameters.AddMember("deltaY", p_m_options->delta_y_, alloc);
  parameters.AddMember("deltaZ", 0, alloc);
    break;
  default:
    break;
  }

  data.AddMember("parameters", parameters, alloc);
  event.AddMember("type", "control", alloc);
  event.AddMember("data", data, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  event.Accept(writer);
  return buffer.GetString();
}

std::string InputEventHandler::OnSizeChange(UINT render_w, UINT render_h)
{
  rapidjson::Document event;
  event.SetObject();
  rapidjson::Document::AllocatorType& alloc = event.GetAllocator();

  float screen_scale_factor = screen_scale_factor_ >= 1.0f ? screen_scale_factor_ : 1.0f;

  int x = (float)(render_w)*screen_scale_factor;
  int y = (float)(render_h)*screen_scale_factor;

  rapidjson::Value renderSize(rapidjson::kObjectType);
  renderSize.SetObject();
  renderSize.AddMember("width", x, alloc);
  renderSize.AddMember("height", y, alloc);

  rapidjson::Value parameters(rapidjson::kObjectType);
  parameters.SetObject();

  parameters.AddMember("mode", "stretch", alloc);
  parameters.AddMember("rendererSize", renderSize, alloc);

  rapidjson::Value data(rapidjson::kObjectType);
  data.AddMember("event", "sizechange", alloc);
  data.AddMember("parameters", parameters, alloc);
  event.AddMember("type", "control", alloc);
  event.AddMember("data", data, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  event.Accept(writer);
  return buffer.GetString();
}

std::string InputEventHandler::onPointerlockchange(bool reativeMode)
{
  rapidjson::Document event;
  event.SetObject();
  rapidjson::Document::AllocatorType& alloc = event.GetAllocator();

  rapidjson::Value parameters(rapidjson::kObjectType);
  parameters.SetObject();
  parameters.AddMember("locked", reativeMode, alloc);

  rapidjson::Value data(rapidjson::kObjectType);
  data.AddMember("event", "pointerlockchange", alloc);
  data.AddMember("parameters", parameters, alloc);
  event.AddMember("type", "control", alloc);
  event.AddMember("data", data, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  event.Accept(writer);
  return buffer.GetString();
}

std::string InputEventHandler::OnStatsRequest(FrameStats* p_framestats)
{
    int64_t ts = p_framestats->ts;
    int64_t size = p_framestats->size;
    int delay = p_framestats->delay;
    int64_t start_delay = p_framestats->start_delay;
    int64_t p_loss = p_framestats->p_loss;
    UINT64 latencymsg = p_framestats->latencymsg_;

    rapidjson::Document event;
    event.SetObject();
    rapidjson::Document::AllocatorType& alloc = event.GetAllocator();

    rapidjson::Value parameters(rapidjson::kObjectType);
    parameters.SetObject();
    parameters.AddMember("framets", ts, alloc);
    parameters.AddMember("framesize", size, alloc);
    parameters.AddMember("framedelay", delay, alloc);
    parameters.AddMember("framestartdelay", start_delay, alloc);
    parameters.AddMember("packetloss", p_loss, alloc);

    if (latencymsg > 0) {
        parameters.AddMember("E2ELatency", latencymsg, alloc);
    }

    rapidjson::Value data(rapidjson::kObjectType);
    data.AddMember("event", "framestats", alloc);
    data.AddMember("parameters", parameters, alloc);
    event.AddMember("type", "control", alloc);
    event.AddMember("data", data, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    event.Accept(writer);
    return buffer.GetString();
}
