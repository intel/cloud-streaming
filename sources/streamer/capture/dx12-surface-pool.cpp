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

#include "dx12-surface-pool.h"
#include "dx12-surface.h"

std::unique_ptr<DX12SurfacePool> DX12SurfacePool::create(DX12SurfacePool::Desc& desc) {
    if (desc.device == nullptr)
        return nullptr;

    auto instance = std::unique_ptr<DX12SurfacePool>(new DX12SurfacePool);
    instance->m_device = desc.device;
    instance->m_heap_props = desc.heap_props;
    instance->m_heap_flags = desc.heap_flags;
    instance->m_resource_desc = desc.resource_desc;

    return instance;
}

std::unique_ptr<Surface> DX12SurfacePool::acquire() {
    std::unique_lock lk(m_acquire_lock);
    for (auto it = m_free_list.begin(); it != m_free_list.end(); /* empty */) {
        Surface* ptr = it->get();
        if (ptr == nullptr) {
            it = m_free_list.erase(it);
            continue;
        }

        HRESULT result = ptr->wait_gpu_event_cpu(0);
        if (result == DXGI_ERROR_WAIT_TIMEOUT) {
            ++it;
            continue;
        }

        auto surface = std::move(*it);
        it = m_free_list.erase(it);
        return surface;
    }
    lk.unlock();

    return DX12Surface::create(m_device, &m_heap_props, m_heap_flags, &m_resource_desc);
}

void DX12SurfacePool::release(std::unique_ptr<Surface> surface) {
    if (surface == nullptr)
        return;

    auto dx12_surface = dynamic_cast<DX12Surface*>(surface.get());
    if (dx12_surface == nullptr)
        return;

    std::lock_guard lk(m_acquire_lock);

    // check if device matches
    if (m_device != dx12_surface->get_device())
        return;
    // check if heap props match
    {
        const auto& lhs = m_heap_props;
        const auto& rhs = dx12_surface->get_heap_props();
        if (lhs.Type != rhs.Type ||
            lhs.CPUPageProperty != rhs.CPUPageProperty ||
            lhs.MemoryPoolPreference != rhs.MemoryPoolPreference ||
            lhs.CreationNodeMask != rhs.CreationNodeMask ||
            lhs.VisibleNodeMask != rhs.VisibleNodeMask)
            return;
    }
    // check if heap flags match
    if (m_heap_flags != dx12_surface->get_heap_flags())
        return;
    // check if resource desc matches
    {
        const auto& lhs = m_resource_desc;
        const auto& rhs = dx12_surface->get_resource_desc();
        if (lhs.Dimension != rhs.Dimension ||
            lhs.Alignment != rhs.Alignment ||
            lhs.Width != rhs.Width ||
            lhs.Height != rhs.Height ||
            lhs.DepthOrArraySize != rhs.DepthOrArraySize ||
            lhs.MipLevels != rhs.MipLevels ||
            lhs.Format != rhs.Format ||
            lhs.SampleDesc.Count != rhs.SampleDesc.Count ||
            lhs.SampleDesc.Quality != rhs.SampleDesc.Quality ||
            lhs.Layout != rhs.Layout ||
            lhs.Flags != rhs.Flags)
            return;
    }

    // move to free list
    m_free_list.push_back(std::move(surface));
}
