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

#include <atlcomcli.h>
#include <d3d11_4.h>
#include <d3d12.h>

#include <deque>
#include <vector>

class EventQueue {
public:
    struct Event {
        HANDLE event_handle = nullptr;
        HANDLE shared_fence = nullptr;
        UINT64 fence_value = 0;
        CComPtr<ID3D11Fence> d3d11_fence;
        CComPtr<ID3D12Fence> d3d12_fence;
        bool signalled = false;
    };

    EventQueue() = default;
    ~EventQueue();

    bool empty() const { return m_event_queue.empty(); }

    Event& front() { return m_event_queue.front(); }
    const Event& front() const { return m_event_queue.front(); }

    Event& back() { return m_event_queue.back(); }
    const Event& back() const { return m_event_queue.back(); }

    void push_back(HANDLE fence, UINT64 value);
    
    void pop_front();

    void flush();

private:
    Event acquire_event(); 

    void release_event(Event& e);

private:
    std::deque<Event> m_event_queue;
    std::vector<Event> m_free_events;
};
