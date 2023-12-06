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

#include "cursor-receiver.h"

#include <chrono>

CursorReceiver::~CursorReceiver() {
    stop();
}

std::unique_ptr<CursorReceiver> CursorReceiver::create(CursorReceiverParams& params) {
    auto instance = std::unique_ptr<CursorReceiver>(new CursorReceiver());

    // set cursor config
    instance->m_params = params;

    return instance;
}

HRESULT CursorReceiver::register_cursor_provider(std::shared_ptr<CursorProvider> provider) {
    std::lock_guard lk(m_acquire_lock);
    // do not register if thread is already active
    if (m_keep_alive)
        return E_ACCESSDENIED;

    m_provider = provider;
    return S_OK;
}

HRESULT CursorReceiver::start() {
    std::lock_guard lk(m_acquire_lock);
    // do not start if thread is already active
    if (m_keep_alive)
        return S_OK;

    // check capture source
    if (m_provider == nullptr) {
        ga_logger(Severity::ERR, "cursor capture provider is nullptr\n");
        return E_FAIL;
    }

    // create thread
    m_keep_alive = true;
    m_thread = std::move(std::thread(CursorReceiver::thread_proc, this));
    return S_OK;
}

void CursorReceiver::stop() {
    std::unique_lock lk(m_acquire_lock);
    if (!m_keep_alive)
        return; // thread is not active

    // signal thread to stop
    m_keep_alive = false;
    // wait for completion
    if (m_thread.joinable())
        m_thread.join();
}

HRESULT CursorReceiver::thread_proc(CursorReceiver* context) {
    using namespace std::chrono_literals;

    struct LogThreadLifetime {
        LogThreadLifetime() { ga_logger(Severity::INFO, "CursorReceiver thread started\n"); }
        ~LogThreadLifetime() { ga_logger(Severity::INFO, "CursorReceiver thread stoped\n"); }
    } log_thread_lifetime;

    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // check capture source
    if (context->m_provider == nullptr) {
        context->on_error(__FUNCTION__ ": cursor capture provider is nullptr", E_INVALIDARG);
        ga_logger(Severity::ERR, __FUNCTION__ ": cursor capture provider is nullptr\n");
        return E_FAIL;
    }

    // cursor state
    CursorState state = {};

    // capture initial state
    context->m_provider->receive_cursor(state, 0);

    const unsigned int timeout_ms = 100; // capture timeout
    while (context->keep_alive()) {
        HRESULT wait_result = context->m_provider->receive_cursor(state, timeout_ms);
        if (wait_result == DXGI_ERROR_WAIT_TIMEOUT)
            continue; // timed out - try again
        else if (FAILED(wait_result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": capture cursor failed, result = 0x%08x\n", wait_result);
            context->on_error(__FUNCTION__ ": capture cursor failed", wait_result);
            continue;
        }

        // notify client that packet is ready
        if (context->m_params.on_cursor_received) {
            context->m_params.on_cursor_received(state);
        }
    }

    return S_OK;
}
