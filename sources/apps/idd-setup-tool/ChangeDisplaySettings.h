// Copyright (C) 2022-2023 Intel Corporation
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

#include "Utility.h"

const UINT32 known_dpi[] = { 100,125,150,175,200,225,250,300,350, 400, 450, 500 };

//our own enum, similar to DISPLAYCONFIG_DEVICE_INFO_TYPE enum in wingdi.h
enum class DISPLAYCONFIG_DEVICE_INFO_TYPE_CUSTOM : int
{
    DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE = -3, //returns min, max, suggested, and currently applied DPI scaling values.
    DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE = -4, //set current dpi scaling value for a display
};

/*
* struct DISPLAYCONFIG_SOURCE_DPI_SCALE_GET
* @brief used to fetch min, max, suggested, and currently applied DPI scaling values.
* All values are relative to the recommended DPI scaling value
* Note that DPI scaling is a property of the source, and not of target.
*/
struct DISPLAYCONFIG_SOURCE_DPI_SCALE_GET
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER            header;
    /*
    * @brief min value of DPI scaling is always 100, minScaleRel gives no. of steps down from recommended scaling
    * eg. if minScaleRel is -3 => 100 is 3 steps down from recommended scaling => recommended scaling is 175%
    */
    int32_t minScaleRel;

    /*
    * @brief currently applied DPI scaling value wrt the recommended value. eg. if recommended value is 175%,
    * => if curScaleRel == 0 the current scaling is 175%, if curScaleRel == -1, then current scale is 150%
    */
    int32_t curScaleRel;

    /*
    * @brief maximum supported DPI scaling wrt recommended value
    */
    int32_t maxScaleRel;
};

/*
* struct DISPLAYCONFIG_SOURCE_DPI_SCALE_SET
* @brief set DPI scaling value of a source
* Note that DPI scaling is a property of the source, and not of target.
*/
struct DISPLAYCONFIG_SOURCE_DPI_SCALE_SET
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER            header;
    /*
    * @brief The value we want to set. The value should be relative to the recommended DPI scaling value of source.
    * eg. if scaleRel == 1, and recommended value is 175% => we are trying to set 200% scaling for the source
    */
    int32_t scaleRel;
};

/*
* struct DPIScalingInfo
* @brief DPI info about a source
* minimum :     minumum DPI scaling in terms of percentage supported by source. Will always be 100%.
* maximum :     maximum DPI scaling in terms of percentage supported by source. eg. 100%, 150%, etc.
* current :     currently applied DPI scaling value
* recommended : DPI scaling value recommended by OS. OS takes resolution, physical size, and expected viewing distance
*               into account while calculating this, however exact formula is not known, hence must be retrieved from OS
*               For a system in which user has not explicitly changed DPI, current should equal recommended.
*/
struct DPIScalingInfo
{
    UINT32 minimum = 100;
    UINT32 maximum = 100;
    UINT32 current = 100;
    UINT32 recommended = 100;
};

void SetDisplayDPI(uint32_t dpi_to_set = 100, bool verbose = true);
void SetDisplayResolution(uint32_t width, uint32_t height);
