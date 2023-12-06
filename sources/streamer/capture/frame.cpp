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

#include "frame.h"

Frame::~Frame() {
    auto pool = m_pool.lock();
    if (pool)
        pool->release(std::move(m_surface));
}

std::unique_ptr<Frame> Frame::create(std::unique_ptr<Surface> surface, std::weak_ptr<SurfacePool> pool) {
    if (surface == nullptr)
        return nullptr;

    auto instance = std::unique_ptr<Frame>(new Frame);
    instance->m_surface = std::move(surface);
    instance->m_pool = pool;
    return instance;
}


