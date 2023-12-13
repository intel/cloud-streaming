// Copyright (C) 2023 Intel Corporation
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

#include "cursor.h"
#include "cursor-sender.h"

std::unique_ptr<CursorSender> CursorSender::create() {
    return std::unique_ptr<CursorSender>(new CursorSender());
}

void CursorSender::on_client_connect() {
    std::lock_guard lk(m_signal_lock);
    m_client_connected = true;
}

void CursorSender::on_client_disconnect() {
    std::lock_guard lk(m_signal_lock);
    m_client_connected = false;
}

void CursorSender::on_resend_cursor() {
    std::lock_guard lk(m_signal_lock);
    send_cursor(m_state);
}

void CursorSender::update_cursor(const CursorState& state) {
    std::unique_lock lk(m_signal_lock);

    // update cached cursor state
    m_state = state;

    // do not send anything if client is not connected
    if (!m_client_connected)
        return;

    lk.unlock();

    send_cursor(state);
}

void CursorSender::send_cursor(const CursorState& state) {
    auto cursor_desc_to_cursor_info = [](const CursorState& state, CURSOR_INFO& info, std::vector<unsigned char>& data) -> void {
        info.isVisible = state.visible;
        if (state.shape_present) {
            info.width = state.shape_width;
            info.height = state.shape_height;
            info.pitch = state.shape_pitch;
            info.isColored = true;
            info.hotSpot.x = state.shape_hotspot_x;
            info.hotSpot.y = state.shape_hotspot_y;
            info.srcRect.left = 0;
            info.srcRect.right = state.shape_width;
            info.srcRect.top = 0;
            info.srcRect.bottom = state.shape_height;
            info.dstRect = info.srcRect; // unused
            data = state.shape_data;
        }
    };

    CURSOR_INFO cursor_info = {};
    std::vector<unsigned char> cursor_data;

    cursor_desc_to_cursor_info(state, cursor_info, cursor_data);

    // send cursor downstream
    if (!cursor_data.empty())
        queue_cursor(cursor_info, cursor_data.data(), cursor_data.size());
    else
        queue_cursor(cursor_info, nullptr, 0);
}
