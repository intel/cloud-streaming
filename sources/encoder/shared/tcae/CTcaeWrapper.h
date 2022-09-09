// Copyright (C) 2018-2022 Intel Corporation
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

#ifndef CTCAEWRAPPER_H
#define CTCAEWRAPPER_H

#include <string>
#include <string.h>
#include <iostream>
#include <vector>
#include <memory>
#include <stdio.h>
#include <map>
#include "enc_frame_settings_predictor.h"

struct FrameData_t
{
    uint32_t targetSize;
    uint32_t encodedSize;
    uint32_t delayInUs;
    uint32_t clientPacketSize;
};

class TcaeLogger
{
 public:
    TcaeLogger();
    ~TcaeLogger();

    inline bool LogEnabled() { return m_enabled; };

    void InitLog(const char* logPath);
    void UpdateClientFeedback(uint32_t delay, uint32_t size);
    void UpdateEncodedSize(uint32_t encodedSize);
    void GetTargetSize(uint32_t targetSize);

protected:

    void makeLogEntry(const FrameData_t& data, const char* str);

    bool m_enabled = false;
    FILE* m_logFilePtr = nullptr;  // File for capturing extra logs in CSV format


    int m_EncFrameNumber = 0;
    int m_FeedbackFrameNumber = 0;
    long long int m_startTime = 0;

    FrameData_t m_encData;
    std::mutex m_mutex;
};

class CTcaeWrapper
{
public:
    CTcaeWrapper() { };
    ~CTcaeWrapper(){ };

    int Initialize(uint32_t targetDelay = 60, uint32_t maxFrameSize = 0);

    int UpdateClientFeedback(uint32_t delay, uint32_t size);

    int UpdateEncodedSize(uint32_t encodedSize);

    uint32_t GetTargetSize();

    void setTcaeLogPath(const char* path) { m_tcaeLogPath = path; };

protected:
    std::unique_ptr<PredictorTcaeImpl> m_tcae;

    const char* m_tcaeLogPath = nullptr;
    TcaeLogger m_logger;
};

#endif /* CTCAEWRAPPER_H */
