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

#include "ga-common.h"
#include "cursor-provider.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct CursorReceiverParams {
    // notification callback that next cursor is ready
    std::function<void(const CursorState&)> on_cursor_received;
    // notification callback that some error has occurred
    std::function<void(const std::string&, HRESULT)> on_error;
};

class CursorReceiver {
public:
    virtual ~CursorReceiver();

    /**
     * @brief      Create instance
     *
     * @return     pointer to new instance, on success
     *             nullptr, on failure
     */
    static std::unique_ptr<CursorReceiver> create(CursorReceiverParams& params);

    /**
     * @brief      Register cursor capture callback
     *
     * @param[in]  callback  pointer to cursor capture callback
     *
     * @return     0, on success
     *             HRESULT, on error
     */
    HRESULT register_cursor_provider(std::shared_ptr<CursorProvider> provider);

    /**
     * @brief      Start cursor receiver
     *
     * @return     0, on success
     *             HRESULT, on error
     */
    HRESULT start();

    /**
     * @brief      Stop cursor receiver
     */
    void stop();

private:
    CursorReceiver() = default;

    bool keep_alive() const { return m_keep_alive.load(); }

    static HRESULT thread_proc(CursorReceiver* context);

    void on_error(std::string msg, HRESULT res) const {
        if (m_params.on_error)
            m_params.on_error(std::move(msg), res);
    }

private:
    // cursor receiver parameters
    CursorReceiverParams m_params = {};

    std::thread m_thread;
    std::atomic<int> m_keep_alive = false;

    std::mutex m_acquire_lock;

    std::shared_ptr<CursorProvider> m_provider = nullptr;
};
