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

#include <ctime>
#include <stdlib.h>
#include "CTcaeWrapper.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::system_clock;

// ------ TcaeLogger class -------

TcaeLogger::TcaeLogger():
    m_enabled(false),
    m_logFilePtr(nullptr),
    m_EncFrameNumber(0),
    m_FeedbackFrameNumber(0),
    m_startTime(0)
{
}

TcaeLogger::~TcaeLogger()
{
    if (m_logFilePtr)
        fclose(m_logFilePtr);
}

void TcaeLogger::InitLog(const char* logPath)
{
    if (logPath == nullptr)
        return;

    m_logFilePtr = fopen(logPath, "w");
    if (!m_logFilePtr)
    {
        printf("Could not open file to write TCAE logs: %s\n", logPath);
        printf("Disabling CTcaeWrapper logs\n");
        m_enabled = false;
    }
    else
    {
        m_enabled = true;

        //Write Headers
        fprintf(m_logFilePtr, "FrameDelay,FrameSize,EncSize,PredSize,Feedback_FrameNumber,EncoderThread_FrameNumber,RelativeTimeStamp,Function\n");
        fflush(m_logFilePtr);
    }

    if (!m_enabled)
        return;

    char* brcOverrideMode = getenv("BRC_OVERRIDE_MODE");
    if (brcOverrideMode)
    {
        if (atoi(brcOverrideMode) == 1)
        {
            m_runVBRmode = true;
            printf("TCAE Logs-Only Override enabled: TCBRC will be off and VBR mode will run with delay + size logs\n");
        }
    }

}

void TcaeLogger::UpdateClientFeedback(uint32_t delay, uint32_t size)
{
    if (!LogEnabled())
        return;

    //This is the last data point for a given frame in its life-cycle
    //This is accessed from the feedback thread.

    FrameData_t frameData;
    frameData.delayInUs = delay;
    frameData.clientPacketSize = size;
    frameData.targetSize = 0;
    frameData.encodedSize = 0;

    makeLogEntry(frameData, __FUNCTION__);

    m_FeedbackFrameNumber++;
}

void TcaeLogger::UpdateEncodedSize(uint32_t encodedSize)
{
    if (!LogEnabled())
        return;

    m_encData.encodedSize = encodedSize;
    makeLogEntry(m_encData, __FUNCTION__);

    //We are now ready to bump the EncFrameNumber counter
    m_EncFrameNumber++;
}

void TcaeLogger::GetTargetSize(uint32_t targetSize)
{
    if (!LogEnabled())
        return;

    //Logging. This is the first data point logged for a given frame.
    //Accessed from the Encode thread
    memset(&m_encData, 0, sizeof(FrameData_t));
    m_encData.targetSize = targetSize;
    makeLogEntry(m_encData, __FUNCTION__);
}

void TcaeLogger::makeLogEntry(const FrameData_t& data, const char* str)
{
    if (!LogEnabled())
        return;

    long long int timestamp = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

    if (m_startTime == 0)
        m_startTime = timestamp;

    {
        std::lock_guard<std::mutex> guard(m_mutex);
        fprintf(m_logFilePtr, "%d, %d, %d, %d, %d, %d, %lld, %s\n",
                data.delayInUs, data.clientPacketSize, data.encodedSize, data.targetSize,
                m_FeedbackFrameNumber, m_EncFrameNumber, (timestamp - m_startTime), str);
    }

    fflush(m_logFilePtr);
}

// ------ CTcaeWrapper class -------
bool CTcaeWrapper::LogsOnlyMode()
{
    return m_logger.LogsOnlyMode();
}

int CTcaeWrapper::Initialize(uint32_t targetDelay, uint32_t maxFrameSize)
{
    try
    {
        m_tcae = std::unique_ptr<PredictorTcaeImpl>(new PredictorTcaeImpl());
    }
    catch (const std::bad_alloc& e)
    {
            return ERR_MEMORY_ALLOC;
    }

    TcaeInitParams_t params = {0};
    params.featuresSet          = TCAE_MODE_STANDALONE;
    params.targetDelayInMs      = targetDelay;
    params.bufferedRecordsCount = 100;
    if (maxFrameSize > 0)
        params.maxFrameSizeInBytes = maxFrameSize;

    tcaeStatus sts = m_tcae->Start(&params);
    if (ERR_NONE != sts)
    {
        printf("Failed to start TCAE.\n");
        return sts;
    }
    else
    {
        printf("################TCAE starts success!################\n");
    }

    m_logger.InitLog(m_tcaeLogPath);

    return 0;
}

int CTcaeWrapper::UpdateClientFeedback(uint32_t delay, uint32_t size)
{
    if (m_tcae == nullptr)
        return ERR_NULL_PTR;

    PerFrameNetworkData_t pfnData = {0};

    pfnData.lastPacketDelayInUs        = delay;
    pfnData.transmittedDataSizeInBytes = size;
    pfnData.packetLossRate             = 0;

    tcaeStatus sts = m_tcae->UpdateNetworkState(&pfnData);
    if (sts != ERR_NONE)
    {
        printf("TCAE: UpdateNetworkState failed with code %d\n", sts);
        return sts;
    }

    m_logger.UpdateClientFeedback(delay, size);

    return 0;
}

int CTcaeWrapper::UpdateEncodedSize(uint32_t encodedSize)
{

    if (m_tcae == nullptr)
        return ERR_NULL_PTR;

    EncodedFrameFeedback_t frameData = {0};
    frameData.encFrameType     = TCAE_FRAMETYPE_UNKNOWN;
    frameData.frameSizeInBytes = encodedSize;

    tcaeStatus sts = m_tcae->BitstreamSent(&frameData);
    if (sts != ERR_NONE)
    {
        printf("TCAE: BitstreamSent is failed with code %d\n", sts);
        return sts;
    }

    m_logger.UpdateEncodedSize(encodedSize);

    return 0;
}

uint32_t CTcaeWrapper::GetTargetSize()
{
    if (m_tcae == nullptr)
        return ERR_NULL_PTR;

    FrameSettings_t settings = {0};

    tcaeStatus sts = m_tcae->PredictEncSettings(&settings);
    if (sts != ERR_NONE)
    {
        printf("TCAE: Failed to predict encode settings. Error code %d\n", sts);
        return sts;
    }

    uint32_t targetSize = settings.frameSizeInBytes;

    m_logger.GetTargetSize(targetSize);

    return targetSize;
}
