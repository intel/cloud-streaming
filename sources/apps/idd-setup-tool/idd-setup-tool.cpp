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

#include "Utility.h"
#include "ChangeDisplaySettings.h"
#include "RearrangeDisplays.h"
#include "PairIdd.h"
#include "EnableDisableAdapters.h"
#include "InstallIDD.h"
#include "IddSetupToolCommandParser.h"

#include <limits>
#include <chrono>
#include <thread>

using namespace std;

extern std::unique_ptr<IddSetupToolCommandParser> g_params;

void ApplyExecutionDelayBeforeDisplayChange(void)
{
    if (g_params->Options.PostActionDelay > 0) {
        tcout << FormatOutput(TEXT("Waiting for %ums before changing display settings..."), g_params->Options.PostActionDelay) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(g_params->Options.PostActionDelay));
    }
}

int main(int argc, char** argv)
{
    int32_t status = 0;
    uint32_t StepCounter = 1;
    size_t num_idd_compatible_adapters = numeric_limits<size_t>::max();
    list_of_settings active_settings;
    bool reboot_required = false;
    bool delay_required_before_display_change = false;

    g_params = std::make_unique<IddSetupToolCommandParser>();

    if (!g_params->ParseCommands(argc, argv)) {
        return -1;
    }

    tcout << FormatOutput(L"Starting IDD Setup Tool") << endl;
    IncIndentation();

    try
    {
        if (g_params->Options.UninstallIdd)
        {
            tcout << FormatOutput(L"%u: Starting uninstallation of previous IDD displays", StepCounter++) << endl;
            IncIndentation();
            if (!UninstallIDD()) {
                return -1;
            }
            DecIndentation();
        }

        if (StringToLowerCase(g_params->Options.ShowAdaptersInfo) == TEXT("yes") || StringToLowerCase(g_params->Options.ShowIddCount) == TEXT("yes"))
        {
            tcout << FormatOutput(L"%u: Showing adapter info", StepCounter++) << endl;
            IncIndentation();

            auto adapter_list = GetAdapterList(StringToLowerCase(g_params->Options.ShowAdaptersInfo) == TEXT("yes") || g_params->Options.Verbose);
            num_idd_compatible_adapters = GetNumIddCompatibleAdapters(adapter_list);

            if (StringToLowerCase(g_params->Options.ShowIddCount) == TEXT("yes"))
            {
                tcout << FormatOutput(L"Number of IDD compatible adapters found on system: %llu", num_idd_compatible_adapters) << endl;
            }

            DecIndentation();
        }

        if (StringToLowerCase(g_params->Options.ShowDisplaysInfo) == TEXT("yes"))
        {
            tcout << FormatOutput(L"%u: Showing display info", StepCounter++) << endl;
            IncIndentation();
            active_settings = QueryActiveSettings(true, true);
            DecIndentation();
        }

        if (g_params->Options.TrustInf)
        {
            tcout << FormatOutput(L"%u: Trusting IDD INF file", StepCounter++) << endl;
            IncIndentation();
            if (!TrustIDD(g_params->Options.InfPath)) {
                return -1;
            }
            DecIndentation();
        }

        if (g_params->Options.InstallIdd)
        {
            tcout << FormatOutput(L"%u: Installing IDD drivers", StepCounter++) << endl;
            tcout << FormatOutputWithOffset(1, L"This will take several seconds and the screen will flash several times. Be patient!") << endl;
            IncIndentation();

            auto adapter_list = GetAdapterList(g_params->Options.Verbose);
            num_idd_compatible_adapters = GetNumIddCompatibleAdapters(adapter_list);

            for (int i = 0; i < num_idd_compatible_adapters; ++i)
            {
                if (!InstallIDD(g_params->Options.InfPath, TEXT("root\\iddsampledriver"), reboot_required, g_params->Options.Verbose)) {
                    return -1;
                }
            }
            tcout << FormatOutput(L"Successfully installed %llu IDD drivers. Reboot is %s", num_idd_compatible_adapters, (reboot_required ? L"REQUIRED" : L"NOT REQUIRED")) << endl;
            DecIndentation();

            tcout << FormatOutput(L"%u: Setting up additional register keys required by IDD", StepCounter++) << endl;
            IncIndentation();
            if (!SetIDDRegisterKeys()) {
                return -1;
            }
            DecIndentation();

            tcout << FormatOutput(L"%u: Restarting all display adapters", StepCounter++) << endl;
            IncIndentation();
            DisableDisplayAdapter(TEXT("idd"), g_params->Options.Verbose);
            DisableDisplayAdapter(TEXT("flex"), g_params->Options.Verbose);
            EnableDisplayAdapter(TEXT("flex"), g_params->Options.Verbose);
            EnableDisplayAdapter(TEXT("idd"), g_params->Options.Verbose);
            DecIndentation();
            delay_required_before_display_change = true;
        }

        if (g_params->Options.PairIdd)
        {
            tcout << FormatOutput(L"%u: Pairing each IDD display adapter with IDD compatible adapter", StepCounter++) << endl;
            IncIndentation();
            PairIddLUIDsToGpuLUIDs();
            DecIndentation();
        }

        if (g_params->Options.DisplaysToDisable.size() > 0)
        {
            if (delay_required_before_display_change) {
                ApplyExecutionDelayBeforeDisplayChange();
                delay_required_before_display_change = false;
            }

            if (g_params->Options.Verbose)
                tcout << FormatOutput(L"%u: Querying Display Info", StepCounter++) << endl;

            IncIndentation();
            active_settings = QueryActiveSettings(g_params->Options.Verbose, true);
            DecIndentation();

            tcout << FormatOutput(L"%u: Disabling requested displays", StepCounter++) << endl;
            IncIndentation();
            for (tstring Pattern : g_params->Options.DisplaysToDisable) {
                tcout << FormatOutput(L"Acting on display pattern: %s", Pattern.c_str()) << endl;

                if (DisableDisplay(Pattern, active_settings)) {
                    return -1;
                }
            }
            DecIndentation();
        }

        if (g_params->Options.AdaptersToDisable.size() > 0)
        {
            tcout << FormatOutput(L"%u: Disabling requested display adapters", StepCounter++) << endl;
            IncIndentation();
            for (tstring Pattern : g_params->Options.AdaptersToDisable) {
                tcout << FormatOutput(L"Acting on display adapter pattern: %s", Pattern.c_str()) << endl;
                DisableDisplayAdapter(Pattern, g_params->Options.Verbose);
            }
            DecIndentation();
            delay_required_before_display_change = true;
        }

        if (g_params->Options.AdaptersToEnable.size() > 0)
        {
            tcout << FormatOutput(L"%u: Enabling requested display adapters", StepCounter++) << endl;
            IncIndentation();
            for (tstring Pattern : g_params->Options.AdaptersToEnable) {
                tcout << FormatOutput(L"Acting on display adapter pattern: %s", Pattern.c_str()) << endl;
                EnableDisplayAdapter(Pattern, g_params->Options.Verbose);
            }
            DecIndentation();
            delay_required_before_display_change = true;
        }

        if (g_params->Options.DisplaysToEnable.size() > 0)
        {
            if (delay_required_before_display_change) {
                ApplyExecutionDelayBeforeDisplayChange();
                delay_required_before_display_change = false;
            }

            if (g_params->Options.Verbose)
                tcout << FormatOutput(L"%u: Querying Display Info", StepCounter++) << endl;

            IncIndentation();
            active_settings = QueryActiveSettings(g_params->Options.Verbose, false);
            DecIndentation();

            tcout << FormatOutput(L"%u: Enabling requested displays", StepCounter++) << endl;
            IncIndentation();
            for (tstring Pattern : g_params->Options.DisplaysToEnable) {
                tcout << FormatOutput(L"Acting on display pattern: %s", Pattern.c_str()) << endl;

                if (EnableDisplay(Pattern, active_settings)) {
                    return -1;
                }
            }
            DecIndentation();
        }

        if (g_params->Options.Resolutions[0] != 0 && g_params->Options.Resolutions[1] != 0)
        {
            if (delay_required_before_display_change) {
                ApplyExecutionDelayBeforeDisplayChange();
                delay_required_before_display_change = false;
            }
            tcout << FormatOutput(L"%u: Adjusting IDD display resolution settings", StepCounter++) << endl;
            IncIndentation();
            SetDisplayResolution(g_params->Options.Resolutions[0], g_params->Options.Resolutions[1]);
            DecIndentation();
        }

        if (g_params->Options.RearrangeDisplays)
        {
            if (delay_required_before_display_change) {
                ApplyExecutionDelayBeforeDisplayChange();
                delay_required_before_display_change = false;
            }

            if (g_params->Options.Verbose)
                tcout << FormatOutput(L"%u: Querying Display Info", StepCounter++) << endl;

            IncIndentation();
            active_settings = QueryActiveSettings(g_params->Options.Verbose, true);
            DecIndentation();

            tcout << FormatOutput(L"%u: Rearranging displays", StepCounter++) << endl;
            IncIndentation();

            if (RearrangeDisplays(active_settings))
                return -1;

            DecIndentation();
        }

        if (g_params->Options.Scale != 0)
        {
            if (delay_required_before_display_change) {
                ApplyExecutionDelayBeforeDisplayChange();
                delay_required_before_display_change = false;
            }

            tcout << FormatOutput(L"%u: Adjusting IDD display scaling settings", StepCounter++) << endl;
            IncIndentation();
            SetDisplayDPI(g_params->Options.Scale, g_params->Options.Verbose);
            DecIndentation();
        }
    }
    catch (const tstring& ex)
    {
        tcout << FormatOutput(L"ERROR: Exception caught!") << endl;
        tcout << FormatOutputWithOffset(1, L"%s", ex.c_str()) << endl;
        return -1;
    }
    catch (...)
    {
        tcout << FormatOutput(L"ERROR: Unexpected exception caught!") << endl;
        return -1;
    }

    DecIndentation();
    tcout << FormatOutput(L"IDD Setup Tool Finished") << endl;
    return 0;
}
