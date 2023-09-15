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

#include "cursor.h"
#include "ga-common.h"
#include "encoder-common.h"

// Queue the cursor data into the server
int queue_cursor(qcsCursorInfoData ciStruct, unsigned char *pBuffer, int nLen, int waitForvideo)
{
    CURSOR_INFO          cursorInfo{};
    cursorInfo.isVisible = (BYTE)ciStruct.isVisible;
    cursorInfo.type = (BYTE)ciStruct.isColored? 2: 1;
    cursorInfo.pos_x     = ciStruct.framePos.x;
    cursorInfo.pos_y     = ciStruct.framePos.y;
    cursorInfo.hotSpot_x = ciStruct.hotSpot.x;
    cursorInfo.hotSpot_y = ciStruct.hotSpot.y;
    cursorInfo.width     = ciStruct.width;
    cursorInfo.height    = ciStruct.height;
    cursorInfo.pitch     = ciStruct.pitch;

    cursorInfo.srcRect.left = ciStruct.srcRect.left;
    cursorInfo.srcRect.right = ciStruct.srcRect.right;
    cursorInfo.srcRect.top = ciStruct.srcRect.top;
    cursorInfo.srcRect.bottom = ciStruct.srcRect.bottom;

    cursorInfo.dstRect.left = ciStruct.dstRect.left;
    cursorInfo.dstRect.right = ciStruct.dstRect.right;
    cursorInfo.dstRect.top = ciStruct.dstRect.top;
    cursorInfo.dstRect.bottom = ciStruct.dstRect.bottom;
    cursorInfo.waitforvideo = waitForvideo;

    //cursorInfo.src
    cursorInfo.lenOfCursor = nLen;

    std::shared_ptr<CURSOR_DATA> cursorData = std::make_shared<CURSOR_DATA>();
    cursorData->cursorInfo = CURSOR_INFO(cursorInfo);  // Copy cursor info for WebRTC channel.
    if (pBuffer) {
        memcpy(cursorData->cursorData, pBuffer, nLen);
        cursorData->cursorDataUpdate = true;
    }
    else {
        cursorData->cursorDataUpdate = false;
    }
    encoder_send_cursor(cursorData, nullptr);

    return 0;
}
