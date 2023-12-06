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

#include "event-queue.h"

EventQueue::~EventQueue() {
    while (!m_event_queue.empty())
        pop_front();

    for (auto& e : m_free_events) {
        if (e.signalled && e.event_handle)
            WaitForSingleObject(e.event_handle, INFINITE);
        if (e.event_handle)
            CloseHandle(e.event_handle);
        if (e.shared_fence)
            CloseHandle(e.shared_fence);
    }
}

void EventQueue::push_back(HANDLE fence, UINT64 value) {
    auto e = acquire_event();
    e.shared_fence = fence;
    e.fence_value = value;
    m_event_queue.push_back(std::move(e));
}

void EventQueue::pop_front() {
    if (m_event_queue.empty())
        return;

    release_event(m_event_queue.front());
    m_event_queue.pop_front();
}

void EventQueue::flush() {
    if (m_event_queue.empty())
        return;

    for (auto it = m_event_queue.begin(); it != m_event_queue.end(); /* empty */) {
        // remove invalid entries
        if (it->event_handle == nullptr) {
            it->signalled = false;
            release_event(*it);
            it = m_event_queue.erase(it);
            continue;
        }

        // try clear signal
        if (it->signalled) {
            auto wait_result = WaitForSingleObject(it->event_handle, 0);
            if (wait_result == WAIT_TIMEOUT) {
                ++it;
                continue; // not signalled yet - skip
            }
            if (wait_result != WAIT_OBJECT_0)
                ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
        
            it->signalled = false;
            release_event(*it);
            it = m_event_queue.erase(it);
            continue;
        }

        ++it;
    }
}

EventQueue::Event EventQueue::acquire_event() {
    if (!m_free_events.empty()) {
        for (auto it = m_free_events.begin(); it != m_free_events.end(); ++it) {
            // remove invalid entries
            if (it->event_handle == nullptr) {
                if (it->shared_fence != nullptr)
                    CloseHandle(it->shared_fence);
                it = m_free_events.erase(it);
                continue;
            }

            // if signalled try to clear signal
            if (it->signalled) {
                auto wait_result = WaitForSingleObject(it->event_handle, 0);
                if (wait_result == WAIT_TIMEOUT)
                    continue; // not signalled yet - skip
                if (wait_result != WAIT_OBJECT_0)
                    ga_logger(Severity::ERR, __FUNCTION__ ": WaitForSingleObject failed, result = 0x%08x\n", wait_result);
            }

            // reset values
            it->signalled = false;
            if (it->shared_fence != nullptr) {
                CloseHandle(it->shared_fence);
                it->shared_fence = nullptr;
            }

            // remove from list
            Event e = std::move(*it);
            it = m_free_events.erase(it);
            return e;
        }
    }

    // create new object
    Event e;
    e.event_handle = CreateEvent(nullptr, false, false, nullptr);
    if (e.event_handle == nullptr) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        ga_logger(Severity::ERR, __FUNCTION__ ": CreateEvent failed, result = 0x%08x\n", result);
        return e;
    }
    return e;
}

void EventQueue::release_event(Event& e) {
    if (!e.signalled) {
        if (e.shared_fence != nullptr) {
            CloseHandle(e.shared_fence);
            e.shared_fence = nullptr;
        }
        e.d3d11_fence = nullptr;
        e.d3d12_fence = nullptr;
    }
    m_free_events.push_back(std::move(e));
}
