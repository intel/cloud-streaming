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

#ifndef _COMMAND_PARSER_IMPL_H_
#define _COMMAND_PARSER_IMPL_H_

#include <unordered_map>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <tuple>
#include <sstream>

#include <windows.h>
#include <Wingdi.h>
#include <Winuser.h>
#include <SetupAPI.h>
#include <stringapiset.h>
#include "Utility.h"

#define MAX_OPTIONS_PER_COMMAND 10
typedef struct COMMAND_STRUCT {
    uint32_t CommandNumber;
    tstring CommandName;
    tstring CommandAbbreviatedDescription;
    tstring CommandDescription;
    tstring CommandBugs;
    bool IsHidden = false;
} CommandStruct;

#define MAX_ARGUMENT_VARIATIONS 3
typedef struct SWITCH_STRUCT {
    bool *Target;
    tstring OptionsList[MAX_ARGUMENT_VARIATIONS];
    uint32_t VariationsCount;
    tstring HelpMessage;
    bool IsHidden = false;
} SwitchStruct;

enum ArgumentTypes {
    NONE           = 0x0,
    STRING         = 0x1,
    INTEGER        = 0x2,
    STRING_VECTOR  = 0x3,
    INTEGER_VECTOR = 0x4,
    PATH           = 0x5,
};

typedef struct PARAM_STRUCT {
    void *Target;
    ArgumentTypes Type = ArgumentTypes::NONE;
    tstring OptionsList[MAX_ARGUMENT_VARIATIONS];
    tstring Delimiter;
    uint32_t ArgumentCount;
    bool IgnoreCase = false;
    uint32_t VariationsCount;
    tstring HelpMessage;
    bool IsHidden = false;
} ParamStruct;

#define HELP_MESSAGE_MAX_LENGTH 110

class CommandParserImpl {
private:
    uint32_t CurrentCommandNumber;
    tstring GlobalDescription;
    tstring GlobalBugs;

    std::unordered_map<tstring, uint32_t> MiscCounters;

    std::vector<CommandStruct> Commands;
    std::unordered_map<uint32_t, tstring> CommandNumberToNameLookup;
    std::unordered_map<tstring, uint32_t> CommandNameToNumberLookup;
    std::vector<SwitchStruct> GlobalSwitches;
    std::vector<ParamStruct> GlobalParams;
    std::unordered_map<tstring, std::vector<ParamStruct>> CommandSpecificSettings;
    std::unordered_map<tstring, std::vector<SwitchStruct>> CommandSpecificSwitches;
    std::unordered_map<tstring, std::vector<ParamStruct>> CommandSpecificParams;

    bool    ExtractAndStoreParamInfo(ParamStruct& Param, tstring Arguments);
    bool    ProcessSingleCommand(tstring CommandLineArgument);
    bool    CheckForGlobalOptions(tstring CommandLineArgument);
    bool    CheckForCommandSpecificOptions(tstring CommandLineArgument);
    bool    CheckForCommandChange(tstring CommandLineArgument);
    tstring FormatHelpMessage(uint32_t Whitespace, tstring Header, tstring Content);
protected:
    tstring ToolName = TEXT("");
    std::vector<tstring> RawArguments;

    CommandStruct& AddCommand(tstring CommandName, tstring CommandAbbreviatedDescription);
    void           AddGlobalDescription(uint32_t OptionCount, ...);
    void           AddCommandSpecificDescription(tstring CommandName, uint32_t OptionCount, ...);
    void           AddGlobalBugs(uint32_t OptionCount, ...);
    void           AddCommandSpecificBugs(tstring CommandName, uint32_t OptionCount, ...);
    SwitchStruct&  AddGlobalSwitch(bool& Switch, tstring HelpMessage, uint32_t OptionCount, ...);
    ParamStruct&   AddGlobalParam(void* Param, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...);
    ParamStruct& AddCommandSpecificSetting(tstring Command, void* Setting, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...);
    SwitchStruct&  AddCommandSpecificSwitch(tstring Command, bool& Switch, tstring HelpMessage, uint32_t OptionCount, ...);
    ParamStruct&   AddCommandSpecificParam(tstring Command, void* Param, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...);
    CommandStruct& SetHidden(CommandStruct& Target);
    SwitchStruct&  SetHidden(SwitchStruct& Target);
    ParamStruct&   SetHidden(ParamStruct& Target);
    CommandStruct* GetCurrentCommand(void);

    // This must be defined by inheriting classes.
    // Handle any fixed constraints like if mode=X this setting must be set, or running A must also run B.
    virtual bool ConstraintFunction(void) = 0;
public:
    CommandParserImpl(void);
    ~CommandParserImpl(void);
    bool ParseCommands(int ArgumentCount, char** Arguments);
    void ShowHelpMessage(void);
};

#endif // _COMMAND_PARSER_IMPL_H_
