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

/**
 * @file   query-adapters.h
 *
 * @brief  Functions for query D3D adapter device information.
 *         Support option to get adapter index by use adapter's LUID.
 *
 */

#pragma once

#include <iostream>
#include <Windows.h>
#include <vector>

 /**
  * @brief AdapterDeviceInfo data structure which collect device information from D3DKMT* data structure.
  */
struct AdapterDeviceInfo
{
    // D3DKMT_DRIVER_DESCRIPTION
    std::wstring    description;    ///< @brief Describes the kernel mode display driver.
    // D3DKMT_GPUVERSION
    std::wstring    gpuBiosVersion; ///< @brief The current bios of the adapter.
    std::wstring    gpuArchitecture;///< @brief The gpu architecture of the adapter.
    // D3DKMT_ADAPTERREGISTRYINFO
    std::wstring    name;           ///< @brief A string that contains the name of the graphics adapter.
    std::wstring    bios;           ///< @brief A string that contains the name of the BIOS for the graphics adapter.
    std::wstring    dacType;        ///< @brief A string that contains the DAC type for the graphics adapter.
    std::wstring    chipType;       ///< @brief A string that contains the chip type for the graphics adapter.
    // D3DKMT_ENUMADAPTERS
    uint32_t        adapterIndex;   ///< @brief Adapter index, based on enumerated order.
    uint32_t        handler;        ///< @brief A handle to the adapter.
    LUID            luid;           /**< @brief A LUID value that uniquely identifies the adapter, typically until
                                                the operating system is rebooted. The LUID value changes whenever:
                                                - the system is rebooted
                                                - the adapter's driver is updated
                                                - the adapter is disabled
                                                - the adapter is disconnected */
    uint32_t        numSources;     ///< @brief The number of video present sources supported by the adapter.
    // D3DKMT_ADAPTERADDRESS
    uint32_t        bus;            ///< @brief The number of the bus that the graphics adapter's physical device is located on.
    uint32_t        device;         ///< @brief The index of the graphics adapter's physical device on the bus.
    uint32_t        function;       ///< @brief The function number of the graphics adapter on the physical device.
    // KMTQAITYPE_ADAPTERGUID
    GUID            guid;           ///< @brief The adapter GUID
    // D3DKMT_SEGMENTSIZEINFO
    uint64_t        vram;           ///< @brief The size, in bytes, of memory that is dedicated from video memory.
    uint64_t        sysRam;         ///< @brief The size, in bytes, of memory that is dedicated from system memory.
    uint64_t        sharedRam;      ///< @brief The size, in bytes, of memory from system memory that can be shared by many users.
    // D3DKMT_PHYSICAL_ADAPTER_COUNT
    uint32_t        physicalAdapterCount;   ///< @brief The physical adapter count.
    // D3DKMT_QUERY_DEVICE_IDS
    uint32_t        physicalAdapterIndex;   ///< @brief The physical adapter index in the LDA (linked display adapter) chain.
    uint32_t        vendorID;       ///< @brief Vendor ID.
    uint32_t        deviceID;       ///< @brief Device ID.
    uint32_t        subVendorID;    ///< @brief Subvendor ID.
    uint32_t        subSystemID;    ///< @brief Subsystem ID.
    uint32_t        revisionID;     ///< @brief Revision ID.
    uint32_t        busType;        ///< @brief Bus type.
    // D3DKMT_ADAPTERTYPE
    union {
        struct {
            uint32_t    renderSupported : 1;            ///< @brief The adapter supports a render device.
            uint32_t    displaySupported : 1;           ///< @brief The adapter supports a display device.
            uint32_t    softwareDevice : 1;             ///< @brief The adapter supports a non-plug and play (PnP) device that is implemented in software.
            uint32_t    postDevice : 1;                 ///< @brief The adapter supports a power-on self-test (POST) device.
            uint32_t    hybridDiscrete : 1;             ///< @brief The adapter supports a hybrid discrete device.
            uint32_t    hybridIntegrated : 1;           ///< @brief The adapter supports a hybrid integrated device.
            uint32_t    indirectDisplayDevice : 1;      ///< @brief The adapter supports an indirect display device.
            uint32_t    paravirtualized : 1;            ///< @brief The adapter supports para-virtualization.
            uint32_t    acgSupported : 1;               ///< @brief The adapter supports Arbitrary Code Guard (ACG).
            uint32_t    supportSetTimingsFromVidPn : 1; ///< @brief The adapter supports set timeing from vidPn.
            uint32_t    detachable : 1;                 ///< @brief The adapter supports a detachable device.
            uint32_t    computeOnly : 1;                ///< @brief The adapter supports a compute-only device.
            uint32_t    prototype : 1;                  ///< @brief The adapter supports a prototype device.
            uint32_t    runtimePowerManagement : 1;     ///< @brief The adapter supports a runtime power management device.
            uint32_t    reserved : 18;                  ///< @brief Reserved for internal use.
        };
        uint32_t        flags;                          ///< @brief The flags used to operate over the other members.
    };
};

typedef std::vector<AdapterDeviceInfo> AdapterDeviceInfoList;

void show_adapter_device_info(const AdapterDeviceInfo& info, bool details = false);
bool check_luid(std::string luidstr, LUID& luid);
bool query_adapters_list(AdapterDeviceInfoList& phyDevices, AdapterDeviceInfoList& indirectDevices, AdapterDeviceInfoList& softwareDevices, bool debug = false);
