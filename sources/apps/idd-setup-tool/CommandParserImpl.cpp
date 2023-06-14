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

#include "CommandParserImpl.h"

CommandParserImpl::CommandParserImpl()
{
    this->CurrentCommandNumber = 0;
}

CommandParserImpl::~CommandParserImpl()
{

}

CommandStruct& CommandParserImpl::AddCommand(tstring CommandName, tstring CommandAbbreviatedDescription)
{
    if (this->MiscCounters.find(TEXT("Commands Offset")) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Commands Offset")] = 0;
    }

    CommandStruct NewCommand{};
    NewCommand.CommandName = CommandName;
    NewCommand.CommandAbbreviatedDescription = CommandAbbreviatedDescription;
    NewCommand.CommandNumber = this->Commands.size() + 1;

    this->Commands.push_back(NewCommand);

    uint32_t ThisOffset = CommandName.size() + 2;

    if (this->MiscCounters[TEXT("Commands Offset")] < ThisOffset) {
        this->MiscCounters[TEXT("Commands Offset")] = ThisOffset;
    }

    this->CommandNumberToNameLookup[NewCommand.CommandNumber] = StringToLowerCase(CommandName);
    this->CommandNameToNumberLookup[StringToLowerCase(CommandName)] = NewCommand.CommandNumber;

    this->CommandSpecificSwitches[StringToLowerCase(CommandName)] = std::vector<SwitchStruct>();
    this->CommandSpecificParams[StringToLowerCase(CommandName)] = std::vector<ParamStruct>();

    return this->Commands.back();
}

void CommandParserImpl::AddGlobalDescription(uint32_t OptionCount, ...)
{
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        if (i != 0) {
            GlobalDescription += TEXT("\n");
        }
        GlobalDescription += arg;
    }
    va_end(args);
}

void CommandParserImpl::AddCommandSpecificDescription(tstring CommandName, uint32_t OptionCount, ...)
{
    uint32_t IndexIntoCommands = this->CommandNameToNumberLookup[StringToLowerCase(CommandName)] - 1;

    CommandStruct *TargetCommand = &this->Commands[IndexIntoCommands];

    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        if (i != 0) {
            TargetCommand->CommandDescription += TEXT("\n");
        }
        TargetCommand->CommandDescription += arg;
    }
    va_end(args);
}

void CommandParserImpl::AddGlobalBugs(uint32_t OptionCount, ...)
{
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        if (i != 0) {
            GlobalBugs += TEXT("\n");
        }
        GlobalBugs += arg;
    }
    va_end(args);
}

void CommandParserImpl::AddCommandSpecificBugs(tstring CommandName, uint32_t OptionCount, ...)
{
    uint32_t IndexIntoCommands = this->CommandNameToNumberLookup[StringToLowerCase(CommandName)] - 1;

    CommandStruct *TargetCommand = &this->Commands[IndexIntoCommands];

    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        if (i != 0) {
            TargetCommand->CommandBugs += TEXT("\n");
        }
        TargetCommand->CommandBugs += arg;
    }
    va_end(args);
}

SwitchStruct& CommandParserImpl::AddGlobalSwitch(bool& Switch, tstring HelpMessage, uint32_t OptionCount, ...)
{
    if (OptionCount > MAX_ARGUMENT_VARIATIONS) {
        tcout
            << FormatOutput(
                TEXT("Command Parser Error: Attempted to add global switch with %u variations. Exceeds limit of %s variations"),
                OptionCount,
                MAX_ARGUMENT_VARIATIONS)
            << std::endl;
        exit(1);
    }

    if (this->MiscCounters.find(TEXT("Global Options Offset")) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Global Options Offset")] = 0;
    }

    SwitchStruct NewSwitch{};
    NewSwitch.Target = &Switch;
    NewSwitch.HelpMessage = HelpMessage;
    NewSwitch.VariationsCount = OptionCount;

    uint32_t ThisOffset = OptionCount * 2;
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        NewSwitch.OptionsList[i] = tstring(arg);
        ThisOffset += NewSwitch.OptionsList[i].size();
    }
    va_end(args);

    this->GlobalSwitches.push_back(NewSwitch);

    if (this->MiscCounters[TEXT("Global Options Offset")] < ThisOffset) {
        this->MiscCounters[TEXT("Global Options Offset")] = ThisOffset;
    }

    return this->GlobalSwitches.back();
}

ParamStruct& CommandParserImpl::AddGlobalParam(void* Param, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...)
{
    if (OptionCount > MAX_ARGUMENT_VARIATIONS) {
        tcout
            << FormatOutput(
                TEXT("Command Parser Error: Attempted to add global param with %u variations. Exceeds limit of %s variations"),
                OptionCount,
                MAX_ARGUMENT_VARIATIONS)
            << std::endl;
        exit(1);
    }

    if (this->MiscCounters.find(TEXT("Global Options Offset")) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Global Options Offset")] = 0;
    }

    ParamStruct NewParam{};
    NewParam.Target = Param;
    NewParam.Type = Type;
    NewParam.Delimiter = Delimiter;
    NewParam.ArgumentCount = ArgumentCount;
    NewParam.IgnoreCase = IgnoreCase;
    NewParam.HelpMessage = HelpMessage;
    NewParam.VariationsCount = OptionCount;

    TCHAR ExpressionTempString[4096];
    swprintf(ExpressionTempString, 4096, TEXT(""));
    if (Type == ArgumentTypes::INTEGER_VECTOR || Type == ArgumentTypes::STRING_VECTOR) {
        swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), Delimiter.c_str(), Delimiter.c_str());
    } else {
        for (int i = 0; i < ArgumentCount; i++) {
            if (i == 0) {
                swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
            } else {
                swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, Delimiter.c_str());
            }
        }
    }
    tstring ExpressionString = tstring(ExpressionTempString);

    uint32_t ThisOffset = ExpressionString.size() + 3;
    uint32_t MaxOptionLength = 0;
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        NewParam.OptionsList[i] = tstring(arg);
        if (NewParam.OptionsList[i].size() > MaxOptionLength) {
            MaxOptionLength = NewParam.OptionsList[i].size();
        }
    }
    va_end(args);
    ThisOffset += MaxOptionLength;

    this->GlobalParams.push_back(NewParam);

    if (this->MiscCounters[TEXT("Global Options Offset")] < ThisOffset) {
        this->MiscCounters[TEXT("Global Options Offset")] = ThisOffset;
    }

    return this->GlobalParams.back();
}

ParamStruct& CommandParserImpl::AddCommandSpecificSetting(tstring Command, void* Setting, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...)
{
    if (OptionCount > MAX_ARGUMENT_VARIATIONS) {
        tcout
            << FormatOutput(
                TEXT("Command Parser Error: Attempted to add command specific (%s) setting with %u variations. Exceeds limit of %s variations"),
                Command.c_str(),
                OptionCount,
                MAX_ARGUMENT_VARIATIONS)
            << std::endl;
        exit(1);
    }

    if (this->MiscCounters.find(TEXT("Command Specific Settings Offset: ") + StringToLowerCase(Command)) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Command Specific Settings Offset: ") + StringToLowerCase(Command)] = 0;
    }

    ParamStruct NewSetting{};
    NewSetting.Target = Setting;
    NewSetting.Type = Type;
    NewSetting.Delimiter = Delimiter;
    NewSetting.ArgumentCount = ArgumentCount;
    NewSetting.IgnoreCase = IgnoreCase;
    NewSetting.HelpMessage = HelpMessage;
    NewSetting.VariationsCount = OptionCount;

    TCHAR ExpressionTempString[4096];
    swprintf(ExpressionTempString, 4096, TEXT(""));
    if (Type == ArgumentTypes::INTEGER_VECTOR || Type == ArgumentTypes::STRING_VECTOR) {
        swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), Delimiter.c_str(), Delimiter.c_str());
    } else {
        for (int i = 0; i < ArgumentCount; i++) {
            if (i == 0) {
                swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
            } else {
                swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, Delimiter.c_str());
            }
        }
    }
    tstring ExpressionString = tstring(ExpressionTempString);

    uint32_t ThisOffset = ExpressionString.size() + 3;
    uint32_t MaxOptionLength = 0;
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        NewSetting.OptionsList[i] = tstring(arg);
        if (NewSetting.OptionsList[i].size() > MaxOptionLength) {
            MaxOptionLength = NewSetting.OptionsList[i].size();
        }
    }
    va_end(args);
    ThisOffset += MaxOptionLength;

    this->CommandSpecificSettings[StringToLowerCase(Command)].push_back(NewSetting);

    if (this->MiscCounters[TEXT("Command Specific Settings Offset: ") + StringToLowerCase(Command)] < ThisOffset) {
        this->MiscCounters[TEXT("Command Specific Settings Offset: ") + StringToLowerCase(Command)] = ThisOffset;
    }

    return this->CommandSpecificSettings[StringToLowerCase(Command)].back();
}

SwitchStruct& CommandParserImpl::AddCommandSpecificSwitch(tstring Command, bool& Switch, tstring HelpMessage, uint32_t OptionCount, ...)
{
    if (OptionCount > MAX_ARGUMENT_VARIATIONS) {
        tcout
            << FormatOutput(
                TEXT("Command Parser Error: Attempted to add command specific (%s) switch with %u variations. Exceeds limit of %s variations"),
                Command.c_str(),
                OptionCount,
                MAX_ARGUMENT_VARIATIONS)
            << std::endl;
        exit(1);
    }

    if (this->MiscCounters.find(TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] = 0;
    }

    SwitchStruct NewSwitch{};
    NewSwitch.Target = &Switch;
    NewSwitch.HelpMessage = HelpMessage;
    NewSwitch.VariationsCount = OptionCount;

    uint32_t ThisOffset = OptionCount * 2;
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        NewSwitch.OptionsList[i] = tstring(arg);
        ThisOffset += NewSwitch.OptionsList[i].size();
    }
    va_end(args);

    this->CommandSpecificSwitches[StringToLowerCase(Command)].push_back(NewSwitch);

    if (this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] < ThisOffset) {
        this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] = ThisOffset;
    }

    return this->CommandSpecificSwitches[StringToLowerCase(Command)].back();
}

ParamStruct& CommandParserImpl::AddCommandSpecificParam(tstring Command, void* Param, ArgumentTypes Type, bool IgnoreCase, tstring Delimiter, uint32_t ArgumentCount, tstring HelpMessage, uint32_t OptionCount, ...)
{
    if (OptionCount > MAX_ARGUMENT_VARIATIONS) {
        tcout
            << FormatOutput(
                TEXT("Command Parser Error: Attempted to add command specific (%s) param with %u variations. Exceeds limit of %s variations"),
                Command.c_str(),
                OptionCount,
                MAX_ARGUMENT_VARIATIONS)
            << std::endl;
        exit(1);
    }

    if (this->MiscCounters.find(TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)) == this->MiscCounters.end()) {
        this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] = 0;
    }

    ParamStruct NewParam{};
    NewParam.Target = Param;
    NewParam.Type = Type;
    NewParam.Delimiter = Delimiter;
    NewParam.ArgumentCount = ArgumentCount;
    NewParam.IgnoreCase = IgnoreCase;
    NewParam.HelpMessage = HelpMessage;
    NewParam.VariationsCount = OptionCount;

    TCHAR ExpressionTempString[4096];
    swprintf(ExpressionTempString, 4096, TEXT(""));
    if (Type == ArgumentTypes::INTEGER_VECTOR || Type == ArgumentTypes::STRING_VECTOR) {
        swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), Delimiter.c_str(), Delimiter.c_str());
    } else {
        for (int i = 0; i < ArgumentCount; i++) {
            if (i == 0) {
                swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
            } else {
                swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, Delimiter.c_str());
            }
        }
    }
    tstring ExpressionString = tstring(ExpressionTempString);

    uint32_t ThisOffset = ExpressionString.size() + 3;
    uint32_t MaxOptionLength = 0;
    va_list args;
    va_start(args, OptionCount);
    for (int i = 0; i < OptionCount; i++) {
        TCHAR *arg = va_arg(args, TCHAR*);
        NewParam.OptionsList[i] = tstring(arg);
        if (NewParam.OptionsList[i].size() > MaxOptionLength) {
            MaxOptionLength = NewParam.OptionsList[i].size();
        }
    }
    va_end(args);
    ThisOffset += MaxOptionLength;

    this->CommandSpecificParams[StringToLowerCase(Command)].push_back(NewParam);

    if (this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] < ThisOffset) {
        this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(Command)] = ThisOffset;
    }

    return this->CommandSpecificParams[StringToLowerCase(Command)].back();
}

CommandStruct& CommandParserImpl::SetHidden(CommandStruct& Target)
{
    Target.IsHidden = true;
    return Target;
}

SwitchStruct& CommandParserImpl::SetHidden(SwitchStruct& Target)
{
    Target.IsHidden = true;
    return Target;
}

ParamStruct& CommandParserImpl::SetHidden(ParamStruct& Target)
{
    Target.IsHidden = true;
    return Target;
}

CommandStruct* CommandParserImpl::GetCurrentCommand(void)
{
    if (this->CurrentCommandNumber == 0) {
        return NULL;
    }

    int32_t IndexIntoCommands = this->CurrentCommandNumber - 1;
    return &this->Commands[IndexIntoCommands];
}

bool CommandParserImpl::ParseCommands(int ArgumentCount, char** Arguments)
{
    int32_t ProcessedCount = 1;
    while (ProcessedCount < ArgumentCount) {
        this->RawArguments.push_back(get_tstring_from_chars(Arguments[ProcessedCount]));
        if (this->ProcessSingleCommand(get_tstring_from_chars(Arguments[ProcessedCount])) == false) {
            tcout << TEXT("Invalid Argument Detected: ") << get_tstring_from_chars(Arguments[ProcessedCount]) << std::endl;
            return false;
        }
        ProcessedCount++;
    }

    // Call child classes constraint function and return result.
    return this->ConstraintFunction();
}

tstring CommandParserImpl::FormatHelpMessage(uint32_t Whitespace, tstring Header, tstring Content)
{
    // Prepare indentation to insert
    tstring IndentationString = TEXT("");
    for (int Indent = 0; Indent < Whitespace; Indent++)
    {
        IndentationString += TEXT(" ");
    }
    // Prepare the header to insert on first line only
    while (Header.length() < Whitespace) {
        Header += TEXT(" ");
    }

    // Iterate over lines in Content and break them into lines of the max allowed length.
    int32_t StartLocation = 0;
    size_t EndLocation = Content.find(TEXT('\n'), StartLocation);
    tstring IntermediateContent = TEXT("");
    uint32_t MaxLineLength = HELP_MESSAGE_MAX_LENGTH - Whitespace;
    while (EndLocation != tstring::npos)
    {
        tstring Substring = Content.substr(StartLocation, EndLocation - StartLocation);
        Substring.erase(remove(begin(Substring), end(Substring), TEXT('\n')), cend(Substring));
        Substring.erase(remove(begin(Substring), end(Substring), TEXT('\r')), cend(Substring));
        while (Substring.length() > MaxLineLength) {
            uint32_t SplitSize = MaxLineLength;

            while (Substring.at(SplitSize + 1) != TEXT(' ')) {
                SplitSize -= 1;
            }
            while (Substring.at(SplitSize) == TEXT(' ')) {
                SplitSize -= 1;
            }
            SplitSize++;
            tstring SplitSubstring = Substring.substr(0, SplitSize + 1);
            Substring = Substring.substr(SplitSize + 1);
            IntermediateContent += SplitSubstring + TEXT("\n");
        }
        IntermediateContent += Substring + TEXT("\n");
        StartLocation = EndLocation + 1;
        EndLocation = Content.find(TEXT('\n'), StartLocation);
    }
    tstring Substring = Content.substr(StartLocation, Content.length() - StartLocation);
    Substring.erase(remove(begin(Substring), end(Substring), TEXT('\n')), cend(Substring));
    Substring.erase(remove(begin(Substring), end(Substring), TEXT('\r')), cend(Substring));
    while (Substring.length() > MaxLineLength + 1) {
        uint32_t SplitSize = MaxLineLength;

        while (Substring.at(SplitSize + 1) != TEXT(' ')) {
            SplitSize -= 1;
        }
        while (Substring.at(SplitSize) == TEXT(' ')) {
            SplitSize -= 1;
        }
        SplitSize++;
        tstring SplitSubstring = Substring.substr(0, SplitSize + 1);
        Substring = Substring.substr(SplitSize + 1);
        IntermediateContent += SplitSubstring + TEXT("\n");
    }
    IntermediateContent += Substring;

    tstring StringToInsert = Header;

    // Iterate over lines in IntermediateContent and insert indentation or the header.
    StartLocation = 0;
    EndLocation = IntermediateContent.find(TEXT('\n'), StartLocation);
    tstring Destination = TEXT("");
    while (EndLocation != tstring::npos)
    {
        tstring Substring = IntermediateContent.substr(StartLocation, EndLocation - StartLocation);
        Substring.erase(remove(begin(Substring), end(Substring), TEXT('\n')), cend(Substring));
        Substring.erase(remove(begin(Substring), end(Substring), TEXT('\r')), cend(Substring));
        Destination += StringToInsert + Substring + TEXT("\n");
        StartLocation = EndLocation + 1;
        EndLocation = IntermediateContent.find(TEXT('\n'), StartLocation);
        StringToInsert = IndentationString;
    }
    Substring = IntermediateContent.substr(StartLocation, IntermediateContent.length() - StartLocation);
    Substring.erase(remove(begin(Substring), end(Substring), TEXT('\n')), cend(Substring));
    Substring.erase(remove(begin(Substring), end(Substring), TEXT('\r')), cend(Substring));
    Destination += StringToInsert + Substring;

    return Destination;
}

void CommandParserImpl::ShowHelpMessage(void)
{
    size_t GlobalOptionCount = this->GlobalSwitches.size() + this->GlobalParams.size();

    if (this->CurrentCommandNumber == 0) {
        tcout << FormatOutput(TEXT("1) %s [<global-options>]"), this->ToolName.c_str()) << std::endl;
        tcout << FormatOutput(TEXT("2) %s <command> [<options>] [<global-options>]"), this->ToolName.c_str()) << std::endl;
        tcout << std::endl;
        tcout << FormatOutput(TEXT("Supported Commands:")) << std::endl;
        tcout << std::endl;

        for (auto Command : this->Commands) {
            if (Command.IsHidden) continue;
            tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(this->MiscCounters[TEXT("Commands Offset")], Command.CommandName, Command.CommandAbbreviatedDescription).c_str()) << std::endl;
        }
    } else {
        int32_t IndexIntoCommands = this->CurrentCommandNumber - 1;

        tstring CurrentCommandName = this->Commands[IndexIntoCommands].CommandName;

        size_t CommandSpecificOptionCount = this->CommandSpecificSwitches[StringToLowerCase(CurrentCommandName)].size() + this->CommandSpecificParams[StringToLowerCase(CurrentCommandName)].size();

        tstring UsageString = FormatString(TEXT("%s %s"), this->ToolName.c_str(), CurrentCommandName.c_str());
        if (this->CommandSpecificSettings[StringToLowerCase(CurrentCommandName)].size() > 0) {
            UsageString += FormatString(TEXT(" <settings> ..."));
        }
        if (CommandSpecificOptionCount > 0) {
            UsageString += FormatString(TEXT(" [<options>]"));
        }
        if (GlobalOptionCount > 0) {
            UsageString += FormatString(TEXT(" [<global-options>]"));
        }

        tcout << FormatOutput(UsageString.c_str()) << std::endl;

        if (this->CommandSpecificSettings[StringToLowerCase(CurrentCommandName)].size() > 0) {
            tcout << std::endl;
            tcout << FormatOutput(TEXT("Settings:")) << std::endl;
            tcout << std::endl;

            uint32_t CommandSpecificSettingsOffset = this->MiscCounters[TEXT("Command Specific Settings Offset: ") + StringToLowerCase(CurrentCommandName)];
            for (auto CommandSpecificSetting : this->CommandSpecificSettings[StringToLowerCase(CurrentCommandName)]) {
                if (CommandSpecificSetting.IsHidden) continue;
                TCHAR ExpressionTempString[4096];
                swprintf(ExpressionTempString, 4096, TEXT(""));
                if (CommandSpecificSetting.Type == ArgumentTypes::INTEGER_VECTOR || CommandSpecificSetting.Type == ArgumentTypes::STRING_VECTOR) {
                    swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), CommandSpecificSetting.Delimiter.c_str(), CommandSpecificSetting.Delimiter.c_str());
                } else {
                    for (int i = 0; i < CommandSpecificSetting.ArgumentCount; i++) {
                        if (i == 0) {
                            swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
                        } else {
                            swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, CommandSpecificSetting.Delimiter.c_str());
                        }
                    }
                }
                tstring ExpressionString = tstring(ExpressionTempString);

                tstring Commands = TEXT("");
                for (uint32_t Iteration = 0; Iteration < CommandSpecificSetting.VariationsCount; Iteration++) {
                    tstring SettingCommand = CommandSpecificSetting.OptionsList[Iteration] + TEXT("=") + ExpressionString;
                    tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(CommandSpecificSettingsOffset, SettingCommand, CommandSpecificSetting.HelpMessage).c_str()) << std::endl;
                }
            }
        }

        if (CommandSpecificOptionCount > 0) {
            tcout << std::endl;
            tcout << FormatOutput(TEXT("Options:")) << std::endl;
            tcout << std::endl;

            uint32_t CommandSpecificOptionsOffset = this->MiscCounters[TEXT("Command Specific Options Offset: ") + StringToLowerCase(CurrentCommandName)];
            for (auto CommandSpecificSwitch : this->CommandSpecificSwitches[StringToLowerCase(CurrentCommandName)]) {
                if (CommandSpecificSwitch.IsHidden) continue;
                tstring Commands = TEXT("");
                for (uint32_t Iteration = 0; Iteration < CommandSpecificSwitch.VariationsCount; Iteration++) {
                    tstring SwitchCommand = CommandSpecificSwitch.OptionsList[Iteration];
                    if (Iteration != 0) {
                        Commands += tstring(TEXT(", "));
                    }
                    Commands += SwitchCommand;
                }
                tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(CommandSpecificOptionsOffset, Commands, CommandSpecificSwitch.HelpMessage).c_str()) << std::endl;
            }

            for (auto CommandSpecificParam : this->CommandSpecificParams[StringToLowerCase(CurrentCommandName)]) {
                if (CommandSpecificParam.IsHidden) continue;
                TCHAR ExpressionTempString[4096];
                swprintf(ExpressionTempString, 4096, TEXT(""));
                if (CommandSpecificParam.Type == ArgumentTypes::INTEGER_VECTOR || CommandSpecificParam.Type == ArgumentTypes::STRING_VECTOR) {
                    swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), CommandSpecificParam.Delimiter.c_str(), CommandSpecificParam.Delimiter.c_str());
                } else {
                    for (int i = 0; i < CommandSpecificParam.ArgumentCount; i++) {
                        if (i == 0) {
                            swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
                        } else {
                            swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, CommandSpecificParam.Delimiter.c_str());
                        }
                    }
                }
                tstring ExpressionString = tstring(ExpressionTempString);

                tstring Commands = TEXT("");
                for (uint32_t Iteration = 0; Iteration < CommandSpecificParam.VariationsCount; Iteration++) {
                    tstring ParamCommand = CommandSpecificParam.OptionsList[Iteration] + TEXT("=") + ExpressionString;
                    tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(CommandSpecificOptionsOffset, ParamCommand, CommandSpecificParam.HelpMessage).c_str()) << std::endl;
                }
            }
        }
    }

    tcout << std::endl;
    tcout << FormatOutput(TEXT("Globally Available Options:")) << std::endl;
    tcout << std::endl;

    uint32_t GlobalOptionsOffset = this->MiscCounters[TEXT("Global Options Offset")];
    for (auto GlobalSwitch : this->GlobalSwitches) {
        if (GlobalSwitch.IsHidden) continue;
        tstring Commands = TEXT("");
        for (uint32_t Iteration = 0; Iteration < GlobalSwitch.VariationsCount; Iteration++) {
            tstring SwitchCommand = GlobalSwitch.OptionsList[Iteration];
            if (Iteration != 0) {
                Commands += tstring(TEXT(", "));
            }
            Commands += SwitchCommand;
        }
        tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(GlobalOptionsOffset, Commands, GlobalSwitch.HelpMessage).c_str()) << std::endl;
    }

    for (auto GlobalParam : this->GlobalParams) {
        if (GlobalParam.IsHidden) continue;
        TCHAR ExpressionTempString[4096];
        swprintf(ExpressionTempString, 4096, TEXT(""));
        if (GlobalParam.Type == ArgumentTypes::INTEGER_VECTOR || GlobalParam.Type == ArgumentTypes::STRING_VECTOR) {
            swprintf(ExpressionTempString, 4096, TEXT("<VALUE>%s<VALUE>%s..."), GlobalParam.Delimiter.c_str(), GlobalParam.Delimiter.c_str());
        } else {
            for (int i = 0; i < GlobalParam.ArgumentCount; i++) {
                if (i == 0) {
                    swprintf(ExpressionTempString, 4096, TEXT("%s<VALUE>"), ExpressionTempString);
                } else {
                    swprintf(ExpressionTempString, 4096, TEXT("%s%s<VALUE>"), ExpressionTempString, GlobalParam.Delimiter.c_str());
                }
            }
        }
        tstring ExpressionString = tstring(ExpressionTempString);

        tstring Commands = TEXT("");
        for (uint32_t Iteration = 0; Iteration < GlobalParam.VariationsCount; Iteration++) {
            tstring ParamCommand = GlobalParam.OptionsList[Iteration] + TEXT("=") + ExpressionString;
            tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(GlobalOptionsOffset, ParamCommand, GlobalParam.HelpMessage).c_str()) << std::endl;
        }
    }

    tstring DescriptionString = TEXT("");

    if (this->CurrentCommandNumber != 0) {
        int32_t IndexIntoCommands = this->CurrentCommandNumber - 1;
        DescriptionString += this->Commands[IndexIntoCommands].CommandDescription;
    } else {
        DescriptionString += this->GlobalDescription;
    }

    if (DescriptionString.length() != 0) {
        tcout << std::endl;
        tcout << FormatOutput(TEXT("Description:")) << std::endl;
        tcout << std::endl;
        tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(0, TEXT(""), DescriptionString).c_str()) << std::endl;
    }

    tstring BugsString = TEXT("");

    if (this->CurrentCommandNumber != 0) {
        int32_t IndexIntoCommands = this->CurrentCommandNumber - 1;
        BugsString += this->Commands[IndexIntoCommands].CommandBugs;
    } else {
        BugsString += this->GlobalBugs;
    }

    if (BugsString.length() != 0) {
        tcout << std::endl;
        tcout << FormatOutput(TEXT("Bugs:")) << std::endl;
        tcout << std::endl;
        tcout << FormatOutputWithOffset(1, TEXT("%s"), this->FormatHelpMessage(0, TEXT(""), BugsString).c_str()) << std::endl;
    }

}

bool CommandParserImpl::ExtractAndStoreParamInfo(ParamStruct& Param, tstring Arguments)
{
    int32_t DelimiterCount = 0;
    size_t DelimiterLocation = 0;
    while (Arguments.size() > 0) {
        size_t NextDelimiterLocation = Arguments.find(Param.Delimiter, DelimiterLocation);
        tstring ArgumentsSegment;

        if (NextDelimiterLocation != tstring::npos && Param.Delimiter.size() > 0) {
            ArgumentsSegment = Arguments.substr(DelimiterLocation, NextDelimiterLocation);
            Arguments = tstring(Arguments.begin() + NextDelimiterLocation + Param.Delimiter.size(), Arguments.end());
        } else {
            ArgumentsSegment = Arguments;
            Arguments = TEXT("");
        }

        if (Param.IgnoreCase) {
            ArgumentsSegment = StringToLowerCase(ArgumentsSegment);
        }

        if (Param.Type == ArgumentTypes::STRING) {
            tstring *Target = (tstring *) Param.Target;
            Target[DelimiterCount] = ArgumentsSegment;
        } else if (Param.Type == ArgumentTypes::INTEGER) {
            int32_t *Target = (int *) Param.Target;
            Target[DelimiterCount] = get_int_from_tstring(ArgumentsSegment);
        } else if (Param.Type == ArgumentTypes::STRING_VECTOR) {
            std::vector<tstring> *Target = (std::vector<tstring> *) Param.Target;
            Target->push_back(ArgumentsSegment);
        } else if (Param.Type == ArgumentTypes::INTEGER_VECTOR) {
            std::vector<int32_t> *Target = (std::vector<int> *) Param.Target;
            Target->push_back(get_int_from_tstring(ArgumentsSegment));
        } else if (Param.Type == ArgumentTypes::PATH) {
            std::filesystem::path* Target = (std::filesystem::path*)Param.Target;
            Target[DelimiterCount] = ArgumentsSegment;
        }
        DelimiterCount++;
    }

    if (Param.ArgumentCount != 0 && DelimiterCount != Param.ArgumentCount) {
        return false;
    } else if (Param.ArgumentCount == 0 && DelimiterCount == 0) {
        return false;
    }

    return true;
}

bool CommandParserImpl::ProcessSingleCommand(tstring CommandLineArgument)
{
    if (this->CheckForGlobalOptions(CommandLineArgument)) {
        return true;
    }

    if (this->CurrentCommandNumber == 0) {
        if (this->CheckForCommandChange(CommandLineArgument)) {
            return true;
        }
    }

    return this->CheckForCommandSpecificOptions(CommandLineArgument);
}

bool CommandParserImpl::CheckForGlobalOptions(tstring CommandLineArgument)
{
    for (auto GlobalParam : this->GlobalParams) {
        for (uint32_t Iteration = 0; Iteration < GlobalParam.VariationsCount; Iteration++) {
            tstring ParamCommand = StringToLowerCase(GlobalParam.OptionsList[Iteration]) + TEXT("=");
            if (StringToLowerCase(CommandLineArgument).find(ParamCommand) != tstring::npos) {
                if (ExtractAndStoreParamInfo(GlobalParam, CommandLineArgument.substr(ParamCommand.size())) == false) {
                    tcout << FormatOutput(TEXT("ERROR: Command line global parameter \"%s\" violated constraints. See --help option."), CommandLineArgument.c_str());
                }
                // No need to reprocess variants of this param.
                Iteration = GlobalParam.VariationsCount;
                return true;
            }
        }
    }

    for (auto GlobalSwitch : this->GlobalSwitches) {
        for (uint32_t Iteration = 0; Iteration < GlobalSwitch.VariationsCount; Iteration++) {
            tstring SwitchCommand = GlobalSwitch.OptionsList[Iteration];
            if (StringToLowerCase(CommandLineArgument) == StringToLowerCase(SwitchCommand)) {
                *GlobalSwitch.Target = true;
                return true;
            }
        }
    }
    return false;
}

bool CommandParserImpl::CheckForCommandSpecificOptions(tstring CommandLineArgument)
{
    int32_t IndexIntoCommands = this->CurrentCommandNumber - 1;
    tstring CurrentCommandName = StringToLowerCase(this->Commands[IndexIntoCommands].CommandName);
    for (auto CommandSpecificSetting : this->CommandSpecificSettings[CurrentCommandName]) {
        for (uint32_t Iteration = 0; Iteration < CommandSpecificSetting.VariationsCount; Iteration++) {
            tstring SettingCommand = StringToLowerCase(CommandSpecificSetting.OptionsList[Iteration]) + TEXT("=");
            if (StringToLowerCase(CommandLineArgument).find(SettingCommand) != tstring::npos) {
                if (ExtractAndStoreParamInfo(CommandSpecificSetting, CommandLineArgument.substr(SettingCommand.size())) == false) {
                    tcout << FormatOutput(TEXT("ERROR: Command line command specific setting \"%s\" violated constraints. See --help option for the current command."), CommandLineArgument.c_str());
                }
                // No need to reprocess variants of this setting.
                Iteration = CommandSpecificSetting.VariationsCount;
                return true;
            }
        }
    }

    for (auto CommandSpecificParam : this->CommandSpecificParams[CurrentCommandName]) {
        for (uint32_t Iteration = 0; Iteration < CommandSpecificParam.VariationsCount; Iteration++) {
            tstring ParamCommand = StringToLowerCase(CommandSpecificParam.OptionsList[Iteration]) + TEXT("=");
            if (StringToLowerCase(CommandLineArgument).find(ParamCommand) != tstring::npos) {
                if (ExtractAndStoreParamInfo(CommandSpecificParam, CommandLineArgument.substr(ParamCommand.size())) == false) {
                    tcout << FormatOutput(TEXT("ERROR: Command line command specific parameter \"%s\" violated constraints. See --help option for the current command."), CommandLineArgument.c_str());
                }
                // No need to reprocess variants of this param.
                Iteration = CommandSpecificParam.VariationsCount;
                return true;
            }
        }
    }

    for (auto CommandSpecificSwitch : this->CommandSpecificSwitches[CurrentCommandName]) {
        for (uint32_t Iteration = 0; Iteration < CommandSpecificSwitch.VariationsCount; Iteration++) {
            tstring SwitchCommand = CommandSpecificSwitch.OptionsList[Iteration];
            if (StringToLowerCase(CommandLineArgument) == StringToLowerCase(SwitchCommand)) {
                *CommandSpecificSwitch.Target = true;
                return true;
            }
        }
    }

    return false;
}

bool CommandParserImpl::CheckForCommandChange(tstring CommandLineArgument)
{
    if (this->CommandNameToNumberLookup.find(StringToLowerCase(CommandLineArgument)) != this->CommandNameToNumberLookup.end()) {
        this->CurrentCommandNumber = this->CommandNameToNumberLookup[StringToLowerCase(CommandLineArgument)];
        return true;
    }
    return false;
}
