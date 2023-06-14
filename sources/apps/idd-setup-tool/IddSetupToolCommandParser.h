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

#include <string>
#include "Utility.h"
#include <vector>
#include "CommandParserImpl.h"

typedef struct IDD_SETUP_TOOL_OPTIONS_STRUCT {
    bool Verbose = false;
    bool Yes = false;
    bool Help = false;
    bool InstallIdd = false;
    bool UninstallIdd = false;
    bool TrustInf = false;
    bool PairIdd = false;
    bool ForceNoUninstall = false;
    bool ForceNoPair = false;
    //TODO: Implement "--display-number" option for "install" command
    //int  IddInstallNumber = -1;
    std::filesystem::path InfPath;
    int32_t Resolutions[2] = { 0 };
    int32_t Scale = 0;
    bool RearrangeDisplays = false;
    std::vector<tstring> AdaptersToDisable { };
    std::vector<tstring> DisplaysToDisable { };
    std::vector<tstring> AdaptersToEnable  { };
    std::vector<tstring> DisplaysToEnable  { };
    //TODO: Implement "--display" option for "set" command
    //tstring TargetDisplay = TEXT("Intel IddSampleDriver Device");
    tstring ShowIddCount = TEXT("");
    tstring ShowAdaptersInfo = TEXT("");
    tstring ShowDisplaysInfo = TEXT("");
    uint32_t IndentationLevel = 0;
    uint32_t PostActionDelay = 2000;
    bool DumpConfigurationValues = false;
} IddSetupToolOptionsStruct;

class IddSetupToolCommandParser : public CommandParserImpl {
protected:
    bool ConstraintFunction(void) override;
    void DumpConfigurationValues(void);
public:
    IddSetupToolCommandParser(void);
    ~IddSetupToolCommandParser(void);

    IddSetupToolOptionsStruct Options;
};
