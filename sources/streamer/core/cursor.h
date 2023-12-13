// Copyright (C) 2022 Intel Corporation
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

#ifndef __CURSOR_H__
#define __CURSOR_H__

#include "ga-common.h"

#define MAX_CURSOR_WIDTH  64
#define MAX_CURSOR_HEIGHT 64
#define MAX_CURSOR_SIZE (MAX_CURSOR_WIDTH * MAX_CURSOR_HEIGHT *4)

struct Rect
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

struct Point
{
    int32_t x;
    int32_t y;
} ;

struct CURSOR_INFO
{
    bool     isVisible;
    bool     isColored;
    Point    pos;
    Point    hotSpot;
    Rect     srcRect;
    Rect     dstRect;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

struct CURSOR_DATA {
    CURSOR_INFO cursorInfo;
    bool        cursorDataUpdate;
    uint32_t    lenOfCursor;
    uint8_t     cursorData[MAX_CURSOR_SIZE];
};

#ifdef WIN32
EXPORT int queue_cursor(const CURSOR_INFO& info, const uint8_t *pBuffer, uint32_t nLen);
#endif

#ifdef WIN32
struct CursorDesc {
    bool visible;
    bool shape_present;
    BITMAP mask = {};
    BITMAP color = {};
    std::vector<unsigned char> mask_data;
    std::vector<unsigned char> color_data;
};
#endif // WIN32

#endif
