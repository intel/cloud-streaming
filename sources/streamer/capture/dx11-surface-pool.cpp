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

#include "dx11-surface-pool.h"
#include "dx11-surface.h"

std::unique_ptr<DX11SurfacePool> DX11SurfacePool::create(DX11SurfacePool::Desc& desc) {
    if (desc.device == nullptr)
        return nullptr;

    auto instance = std::unique_ptr<DX11SurfacePool>(new DX11SurfacePool);
    instance->m_device = desc.device;
    instance->m_texture_desc = desc.texture_desc;

    return instance;
}

std::unique_ptr<Surface> DX11SurfacePool::acquire() {
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

    return DX11Surface::create(m_device, &m_texture_desc);
}

void DX11SurfacePool::release(std::unique_ptr<Surface> surface) {
    if (surface == nullptr)
        return;

    auto dx11_surface = dynamic_cast<DX11Surface*>(surface.get());
    if (dx11_surface == nullptr)
        return;

    std::lock_guard lk(m_acquire_lock);

    // check if device matches
    if (m_device != dx11_surface->get_device())
        return;

    // check if desc matches
    {
        const auto& lhs = m_texture_desc;
        const auto& rhs = dx11_surface->get_texture_desc();
        if (lhs.Width != rhs.Width ||
            lhs.Height != rhs.Height ||
            lhs.MipLevels != rhs.MipLevels ||
            lhs.ArraySize != rhs.ArraySize ||
            lhs.Format != rhs.Format ||
            lhs.SampleDesc.Count != rhs.SampleDesc.Count ||
            lhs.SampleDesc.Quality != rhs.SampleDesc.Quality ||
            lhs.Usage != rhs.Usage ||
            lhs.BindFlags != rhs.BindFlags ||
            lhs.CPUAccessFlags != rhs.CPUAccessFlags ||
            lhs.MiscFlags != rhs.MiscFlags)
            return;
    }

    // move to free list
    m_free_list.push_back(std::move(surface));
}

