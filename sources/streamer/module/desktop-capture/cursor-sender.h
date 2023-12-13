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

#pragma once

#include <memory>
#include <mutex>

#include "cursor-provider.h"

class CursorSender {
public:
    virtual ~CursorSender() = default;

    /**
     * @brief      Create instance
     *
     * @return     pointer to new instance, on success
     *             nullptr, on failure
     */
    static std::unique_ptr<CursorSender> create();

    /**
     * @brief      Client connect event callback
     */
    void on_client_connect();

    /**
     * @brief      Client disconnect event callback
     */
    void on_client_disconnect();

    /**
     * @brief      Resend cursor event callback
     */
    void on_resend_cursor();

    /**
     * @brief      Update local cursor state and send it
     *             downstream
     */
    void update_cursor(const CursorState& state);

private:
    CursorSender() = default;

    void send_cursor(const CursorState& state);

private:
    std::mutex m_signal_lock;

    bool m_resend_cursor = false;
    bool m_client_connected = false;

    // cached cursor state
    CursorState m_state;
};
