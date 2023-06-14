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

#include "IddSetupToolCommandParser.h"
#include <filesystem>
#include <fstream>

extern std::unique_ptr<IddSetupToolCommandParser> g_params;

IddSetupToolCommandParser::IddSetupToolCommandParser()
{
    this->ToolName = TEXT("idd-setup-tool.exe");

    this->AddGlobalSwitch(this->Options.Verbose                                                            , TEXT("Turn on additional logging (default: off)")                                                                             , 2, TEXT("-v"), TEXT("--verbose"));
    this->AddGlobalSwitch(this->Options.Help                                                               , TEXT("Print help")                                                                                                            , 2, TEXT("-h"), TEXT("--help")   );
    this->AddGlobalSwitch(this->Options.Yes                                                                , TEXT("Assume \"yes\" on all prompts (default: off)")                                                                          , 2, TEXT("-y"), TEXT("--yes")    );
    this->AddGlobalParam ((void*) &this->Options.PostActionDelay, ArgumentTypes::INTEGER, true, TEXT(""), 1, TEXT("Applies the specified delay (in milliseconds) after every action that changes display or adapter states (default: 2000ms)"), 1, TEXT("--delay"));

    this->SetHidden(this->AddGlobalParam(
        (void*) &this->Options.IndentationLevel,
        ArgumentTypes::INTEGER,
        true, TEXT(""),
        1,
        TEXT("Offset all output from this tool by this amount"),
        1,
        TEXT("--indentation")
    ));

    this->SetHidden(this->AddGlobalSwitch(
        this->Options.DumpConfigurationValues,
        TEXT("Causes tool to append values of all configuration options interpreted by this command parser to the file idd_setup_tool_dumped_configuration_values.csv."),
        1,
        TEXT("--dump-configuration-values")
    ));

    this->AddGlobalBugs(
        6,
        TEXT("Some functions of this tool do not work in non-interactive shells. A list of known commands that require interactive shells is below; but overall this tool is not validated in non-interactive shell."),
        FormatString(TEXT(" - \"%s set\" scaling change functionality."), this->ToolName.c_str()).c_str(),
        FormatString(TEXT(" - \"%s install\" scaling change functionality."), this->ToolName.c_str()).c_str(),
        TEXT(""),
        TEXT("Some functions of this tool have a known instability when running operations immediately after enabling or disabling adapters."),
        TEXT("In our testing this is resolved by including a small delay between sensitive actions which is applied by default (2000ms) if an adapter has been disabled or enabled. However this can be set to zero or increased if required by using the \"--delay=<VALUE>\" parameter.")
    );

    this->AddCommand(TEXT("install")    , TEXT("Installs IDD")                         );
    this->AddCommand(TEXT("uninstall")  , TEXT("Uninstalls IDD")                       );
    this->AddCommand(TEXT("set")        , TEXT("Configure adapter(s) settings")        );
    this->AddCommand(TEXT("enable")     , TEXT("Enable adapters or displays")          );
    this->AddCommand(TEXT("disable")    , TEXT("Disable adapters or displays")         );
    this->AddCommand(TEXT("pair")       , TEXT("Pair adapters to IDD displays")        );
    this->AddCommand(TEXT("rearrange")  , TEXT("Rearrange available displays")        );
    this->AddCommand(TEXT("show")       , TEXT("Show information related to IDD setup"));

    this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.InfPath          , ArgumentTypes::PATH         , false, TEXT("") , 1, TEXT("Location of IDD driver (IddSampleDriver.inf) to install (default: $bindir\\idd\\)"), 1, TEXT("--location"));
    this->AddCommandSpecificSwitch (TEXT("install")  , this->Options.TrustInf                                                               , TEXT("Extract certificate and add it to the trusted store (default: no)")                                                                  , 1, TEXT("--trust"));
    //TODO: Implement "--display-number" option for "install" command
    //this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.IddInstallNumber , ArgumentTypes::INTEGER      , true , TEXT("") , 1, TEXT("Specify number of IDD displays to install, maximum of 1 per IDD compatible adapter (default: 1 per adapter)")                        , 1, TEXT("--display-number"));
    this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.Scale            , ArgumentTypes::INTEGER      , true , TEXT("") , 1, TEXT("Configure specified scaling for the display (default: use system default)")                                                          , 1, TEXT("--scale"));
    this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.Resolutions[0]   , ArgumentTypes::INTEGER      , true , TEXT("x"), 2, TEXT("Configure specified resolution for the display (default: use system default)")                                                       , 1, TEXT("--resolution"));
    this->AddCommandSpecificSwitch (TEXT("install")  , this->Options.RearrangeDisplays                                                            , TEXT("Rearrange displays horizontally, set leftmost as primary")                                                                           , 1, TEXT("--rearrange"));
    this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.AdaptersToDisable, ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Disable specified adapter (options: msft, idd, flex)")                                                                               , 1, TEXT("--disable-adapter"));
    this->AddCommandSpecificParam  (TEXT("install")  , (void*) &this->Options.DisplaysToDisable, ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Disable specified display (options: non-flex, msft, idd, virtio, non-idd)")                                                          , 1, TEXT("--disable-display"));
    this->SetHidden(
        this->AddCommandSpecificSwitch(TEXT("install"), this->Options.ForceNoUninstall                                                            , TEXT("Perform installation with no uninstall step")                                                                                        , 1, TEXT("--force-no-uninstall"))
    );
    this->SetHidden(
        this->AddCommandSpecificSwitch(TEXT("install"), this->Options.ForceNoPair                                                                 , TEXT("Perform installation with no pair step")                                                                                            , 1, TEXT("--force-no-pair"))
    );

    this->AddCommandSpecificParam  (TEXT("uninstall"), (void*) &this->Options.AdaptersToEnable , ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Enable specified adapter after IDD uninstallation (options: msft, idd, flex)")                                                       , 1, TEXT("--enable-adapter"));
    this->AddCommandSpecificParam  (TEXT("uninstall"), (void*) &this->Options.DisplaysToEnable , ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Enable specified display after IDD uninstallation (options: non-flex, msft, idd, virtio, non-idd)")                                  , 1, TEXT("--enable-display"));

    //TODO: Implement "--display" option for "set" command
    //this->AddCommandSpecificParam  (TEXT("set")      , (void*) &this->Options.TargetDisplay    , ArgumentTypes::STRING       , false, TEXT("") , 1, TEXT("Apply settings for specific display (default: Intel IddSampleDriver Device)")                                                        , 1, TEXT("--display"));
    this->AddCommandSpecificSetting(TEXT("set")      , (void*) &this->Options.Scale            , ArgumentTypes::INTEGER      , true , TEXT("") , 1, TEXT("Configure specified scaling for the display")                                                                                        , 1, TEXT("scale"));
    this->AddCommandSpecificSetting(TEXT("set")      , (void*) &this->Options.Resolutions[0]   , ArgumentTypes::INTEGER      , true , TEXT("x"), 2, TEXT("Configure specified resolution for the display")                                                                                     , 1, TEXT("resolution"));

    this->AddCommandSpecificSetting(TEXT("enable")   , (void*) &this->Options.AdaptersToEnable , ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Enable specified adapter (options: msft, idd, flex)")                                                                               , 1, TEXT("adapter"));
    this->AddCommandSpecificSetting(TEXT("enable")   , (void*) &this->Options.DisplaysToEnable , ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Enable specified display (options: non-flex, msft, idd, virtio, non-idd)")                                                          , 1, TEXT("display"));

    this->AddCommandSpecificSetting(TEXT("disable")  , (void*) &this->Options.AdaptersToDisable, ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Disable specified adapter (options: msft, idd, flex)")                                                                               , 1, TEXT("adapter"));
    this->AddCommandSpecificSetting(TEXT("disable")  , (void*) &this->Options.DisplaysToDisable, ArgumentTypes::STRING_VECTOR, true , TEXT(","), 0, TEXT("Disable specified display (options: non-flex, msft, idd, virtio, non-idd)")                                                          , 1, TEXT("display"));

    this->AddCommandSpecificParam  (TEXT("show")     , (void*) &this->Options.ShowIddCount     , ArgumentTypes::STRING       , true , TEXT("") , 1, TEXT("If this option is \"yes\" then print number of IDD-compatible adapters on the system (default: yes)")                                , 1, TEXT("--count"));
    this->AddCommandSpecificParam  (TEXT("show")     , (void*) &this->Options.ShowAdaptersInfo , ArgumentTypes::STRING       , true , TEXT("") , 1, TEXT("If this option is \"yes\" then print some information on adapters (default: yes)")                                                   , 1, TEXT("--adapters"));
    this->AddCommandSpecificParam  (TEXT("show")     , (void*) &this->Options.ShowDisplaysInfo , ArgumentTypes::STRING       , true , TEXT("") , 1, TEXT("If this option is \"yes\" then print some information on displays (default: yes)")                                                   , 1, TEXT("--displays"));

    this->AddGlobalDescription(
        3,
        TEXT("The order in which arguments are supplied to this tool is not important, with the exception of the <command> argument which must come before any arguments specific to that command."),
        TEXT(""),
        TEXT("All options and modes in this tool are case insensitive; though the values provided to these options (e.g. a path to an INF file) may still be case sensitive.")
    );

    this->AddCommandSpecificDescription(
        TEXT("install"),
        12,
        TEXT("Installs IDD display for each IDD compatible adapter."),
        TEXT("This will uninstall any previously installed IDD displays prior to installation of new IDD displays."),
        TEXT(""),
        TEXT("Pairing is done automatically following installation."),
        TEXT("See \"idd-setup-tool.exe pair --help\" for more details on pairing."),
        TEXT(""),
        TEXT("Using \"--resolution\" or \"--scale\" you can configure any installed IDD displays resolution."),
        TEXT("This behaves identical to \"idd-setup-tool.exe set\" in terms of behavior."),
        TEXT("See \"idd-setup-tool.exe set --help\" for more details on setting IDD resolution and scaling."),
        TEXT(""),
        TEXT("Using \"--disable-adapter\" or \"--disable-display\" you can disable specific adapters or displays specified by the pattern."),
        TEXT("See \"idd-setup-tool.exe disable --help\" for more details on adapter and display disabling, as well as the detailed list of supported patterns.")
    );
    this->AddCommandSpecificDescription(
        TEXT("uninstall"),
        4,
        TEXT("Uninstalls any detected IDD display.")
        TEXT(""),
        TEXT("Using \"--enable-adapter\" or \"--enable-display\" you can enable specific adapters or displays specified by the pattern."),
        TEXT("See \"idd-setup-tool.exe enable --help\" for more details on adapter and display enabling, as well as the detailed list of supported patterns.")
    );
    this->AddCommandSpecificDescription(
        TEXT("set"),
        6,
        TEXT("Configures given setting for available IDD displays."),
        TEXT(""),
        TEXT("Mind that if the order in which these settings are set is important, this should be done by execution of \"idd-setup-tool.exe set\" a few times consecutively"),
        TEXT("Default ordering of this tool is:"),
        TEXT(" - Resolution"),
        TEXT(" - Scaling")
    );
    this->AddCommandSpecificDescription(
        TEXT("enable"),
        15,
        TEXT("Enables specified adapters/displays."),
        TEXT(""),
        TEXT("Using \"adapter\" or \"display\" you can enable adapters or displays specified by a pattern:"),
        TEXT(" - non-flex = All except Intel Data Center GPU Flex"),
        TEXT(" - msft     = Microsoft Basic Adapter"),
        TEXT(" - virtio   = Red Hat VirtIO GPU DOD controller and Red Hat QXL controller"),
        TEXT(" - idd      = Intel IddSampleDriver Device"),
        TEXT(" - flex     = Intel Data Center GPU Flex Series"),
        TEXT(" - non-idd  = All except Intel IddSampleDriver Device"),
        TEXT("Specific displays and adapters can be targeted by supplying one of the above patterns with an index (e.g. idd1 disable only the first idd display or adapter)."),
        TEXT("'show' command can be used to enumerate the full list of patterns that will affect each adapter or display."),
        TEXT("Default ordering of this tool is:"),
        TEXT(" - Enable adapters"),
        TEXT(" - Enable displays"),
        TEXT("Note: Some of these patterns are display or adapter only.")
    );
    this->AddCommandSpecificDescription(
        TEXT("disable"),
        15,
        TEXT("Disables specified adapters/displays."),
        TEXT(""),
        TEXT("Using \"adapter\" or \"display\" you can disable adapters or displays specified by a pattern:"),
        TEXT(" - non-flex = All except Intel Data Center GPU Flex"),
        TEXT(" - msft     = Microsoft Basic Adapter"),
        TEXT(" - virtio   = Red Hat VirtIO GPU DOD controller and Red Hat QXL controller"),
        TEXT(" - idd      = Intel IddSampleDriver Device"),
        TEXT(" - flex     = Intel Data Center GPU Flex Series"),
        TEXT(" - non-idd  = All except Intel IddSampleDriver Device"),
        TEXT("Specific displays and adapters can be targeted by supplying one of the above patterns with an index (e.g. idd1 disable only the first idd display or adapter)."),
        TEXT("'show' command can be used to enumerate the full list of patterns that will affect each adapter or display."),
        TEXT("Default ordering of this tool is:"),
        TEXT(" - Disable adapters"),
        TEXT(" - Disable displays"),
        TEXT("Note: Some of these patterns are display or adapter only.")
    );
    this->AddCommandSpecificDescription(
        TEXT("pair"),
        2,
        TEXT("Loops through IDD displays and adapters and pairs each IDD display with next IDD compatible adapter."),
        TEXT("If there are more displays than adapters, adapters loop starts anew so some adapters will be assigned with few displays.")
    );
    this->AddCommandSpecificBugs(
        TEXT("pair"),
        9,
        TEXT("This command will likely re-enable MSFT basic display (and perhaps any other disabled displays)."),
        TEXT("It is recommended to disable any unwanted displays after running this command - or any other commands that run"),
        TEXT("IDD paring such as install."),
        TEXT(""),
        TEXT("Pairing should preserve over reboot. Its our bug that it's not. As a workaround \"pair\" command must be re-run"),
        TEXT("anytime the following occurs:"),
        TEXT(" - IDD Driver is disabled/enabled"),
        TEXT(" - GFX Driver is disabled/enabled"),
        TEXT(" - System is rebooted")
    );
    this->AddCommandSpecificDescription(
        TEXT("rearrange"),
        1,
        TEXT("Rearranges displays horizontally, and sets the leftmost display as the primary.")
    );
    this->AddCommandSpecificDescription(
        TEXT("show"),
        2,
        TEXT("Prints information on adapters or displays."),
        TEXT("Along with information about each display and adapter the list of adapter and display enabling and disabling patterns that will affect each device is also displayed.")
    );
}

IddSetupToolCommandParser::~IddSetupToolCommandParser()
{

}

bool IddSetupToolCommandParser::ConstraintFunction(void)
{
    CommandStruct *CurrentCommand = this->GetCurrentCommand();

    // If just asking for help call help function and exit with return code 0.
    // Also show help message is no [COMMAND] was specified.
    if (this->Options.Help || CurrentCommand == NULL) {
        this->ShowHelpMessage();
        exit(0);
    }

    // Set default modes enabled depending on the mode its running in.
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("install")) {
        if (this->Options.ForceNoUninstall == false) {
            this->Options.UninstallIdd = true;
        }
        this->Options.InstallIdd = true;
        if (this->Options.ForceNoPair == false) {
            this->Options.PairIdd = true;
        }
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("uninstall")) {
        this->Options.UninstallIdd = true;
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("set")) {
        if (this->Options.Resolutions[0] == 0 && this->Options.Scale == 0) {
            tcout << FormatOutput(TEXT("Error: At least one setting must be provided to \"set\" command.")) << std::endl;
            tcout << FormatOutput(TEXT("       See \"idd-setup-tool.exe set --help\" for a list of available settings.")) << std::endl;
            exit(-1);
        }
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("enable")) {
        if (this->Options.AdaptersToEnable.size() == 0 && this->Options.DisplaysToEnable.size() == 0) {
            tcout << FormatOutput(TEXT("Error: At least one setting must be provided to \"enable\" command.")) << std::endl;
            tcout << FormatOutput(TEXT("       See \"idd-setup-tool.exe enable --help\" for a list of available settings.")) << std::endl;
            exit(-1);
        }
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("disable")) {
        if (this->Options.AdaptersToDisable.size() == 0 && this->Options.DisplaysToDisable.size() == 0) {
            tcout << FormatOutput(TEXT("Error: At least one setting must be provided to \"disable\" command.")) << std::endl;
            tcout << FormatOutput(TEXT("       See \"idd-setup-tool.exe disable --help\" for a list of available settings.")) << std::endl;
            exit(-1);
        }
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("pair")) {
        this->Options.PairIdd = true;
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("rearrange")) {
        this->Options.RearrangeDisplays = true;
    }
    if (StringToLowerCase(CurrentCommand->CommandName) == TEXT("show")) {
        if (this->Options.ShowAdaptersInfo == TEXT("")) {
            this->Options.ShowAdaptersInfo = TEXT("yes");
        }
        if (this->Options.ShowDisplaysInfo == TEXT("")) {
            this->Options.ShowDisplaysInfo = TEXT("yes");
        }
        if (this->Options.ShowIddCount == TEXT("")) {
            this->Options.ShowIddCount = TEXT("yes");
        }
    }

    // Current usage limitations: if certain switches are enabled others must be as well.
    if (this->Options.InstallIdd   ||
        this->Options.TrustInf) {
        std::filesystem::path& idd = this->Options.InfPath;

        if (idd.empty()) {
            idd = GetDefaultIddPath();
        }
        if (!isIddOk(idd)) {
            tcout << FormatOutput(TEXT("Error: No IDD files (.inf, .dll, .cat) found in %s"), idd.c_str()) << std::endl;
            exit(-1);
        }
        tcout << FormatOutput(TEXT("Found IDD files (.inf, .dll, .cat) to install: %s"), idd.c_str()) << std::endl;
    }

    if (this->Options.DumpConfigurationValues) {
        this->DumpConfigurationValues();
    }

    return true;
}

void IddSetupToolCommandParser::DumpConfigurationValues(void)
{
    std::ofstream MyFile;
    if (std::filesystem::exists(TEXT("idd_setup_tool_dumped_configuration_values.csv")) == false) {
        MyFile.open(TEXT("idd_setup_tool_dumped_configuration_values.csv"), std::ios::out);
        MyFile << "CL";
        MyFile << "," << "Verbose";
        MyFile << "," << "Yes";
        MyFile << "," << "Help";
        MyFile << "," << "InstallIdd";
        MyFile << "," << "UninstallIdd";
        MyFile << "," << "TrustInf";
        MyFile << "," << "PairIdd";
        MyFile << "," << "ForceNoUninstall";
        MyFile << "," << "ForceNoPair";
        //TODO: Implement "--display-number" option for "install" command
        //MyFile << "," << "IddInstallNumber";
        MyFile << "," << "InfPath";
        MyFile << "," << "Resolutions.Width";
        MyFile << "," << "Resolutions.Height";
        MyFile << "," << "Scale";
        MyFile << "," << "RearrangeDisplays";
        MyFile << "," << "AdaptersToDisable";
        MyFile << "," << "DisplaysToDisable";
        MyFile << "," << "AdaptersToEnable";
        MyFile << "," << "DisplaysToEnable";
        //TODO: Implement "--display" option for "set" command
        //MyFile << "," << "TargetDisplay";
        MyFile << "," << "ShowIddCount";
        MyFile << "," << "ShowAdaptersInfo";
        MyFile << "," << "ShowDisplaysInfo";
        MyFile << "," << "IndentationLevel";
        MyFile << "," << "PostActionDelay";
        MyFile << std::endl;
    } else {
        MyFile.open(TEXT("idd_setup_tool_dumped_configuration_values.csv"), std::ios::out | std::ios::app);
    }

    for (int i = 0; i < this->RawArguments.size(); i++) {
        if (i != 0) {
            MyFile << " ";
        }
        MyFile << ws2s(this->RawArguments[i]);
    }

    MyFile << "," << this->Options.Verbose;
    MyFile << "," << this->Options.Yes;
    MyFile << "," << this->Options.Help;
    MyFile << "," << this->Options.InstallIdd;
    MyFile << "," << this->Options.UninstallIdd;
    MyFile << "," << this->Options.TrustInf;
    MyFile << "," << this->Options.PairIdd;
    MyFile << "," << this->Options.ForceNoUninstall;
    MyFile << "," << this->Options.ForceNoPair;
    //TODO: Implement "--display-number" option for "install" command
    //MyFile << "," << this->Options.IddInstallNumber;
    MyFile << "," << this->Options.InfPath.c_str();
    MyFile << "," << this->Options.Resolutions[0];
    MyFile << "," << this->Options.Resolutions[1];
    MyFile << "," << this->Options.Scale;
    MyFile << "," << this->Options.RearrangeDisplays;
    MyFile << ",";
    for (int Index = 0; Index < this->Options.AdaptersToDisable.size(); Index++) {
        if (Index != 0) {
            MyFile << "|";
        }
         MyFile << ws2s(this->Options.AdaptersToDisable[Index]);
    }
    MyFile << ",";
    for (int Index = 0; Index < this->Options.DisplaysToDisable.size(); Index++) {
        if (Index != 0) {
            MyFile << "|";
        }
         MyFile << ws2s(this->Options.DisplaysToDisable[Index]);
    }
    MyFile << ",";
    for (int Index = 0; Index < this->Options.AdaptersToEnable.size(); Index++) {
        if (Index != 0) {
            MyFile << "|";
        }
         MyFile << ws2s(this->Options.AdaptersToEnable[Index]);
    }
    MyFile << ",";
    for (int Index = 0; Index < this->Options.DisplaysToEnable.size(); Index++) {
        if (Index != 0) {
            MyFile << "|";
        }
         MyFile << ws2s(this->Options.DisplaysToEnable[Index]);
    }
    //TODO: Implement "--display" option for "set" command
    //MyFile << "," << ws2s(this->Options.TargetDisplay);
    MyFile << "," << ws2s(this->Options.ShowIddCount);
    MyFile << "," << ws2s(this->Options.ShowAdaptersInfo);
    MyFile << "," << ws2s(this->Options.ShowDisplaysInfo);
    MyFile << "," << this->Options.IndentationLevel;
    MyFile << "," << this->Options.PostActionDelay;
    MyFile << std::endl;

    MyFile.close();
}
