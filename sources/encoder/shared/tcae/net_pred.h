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

#ifndef __NET_PRED_H__
#define __NET_PRED_H__

#include <stdint.h>
#include <deque>
#include <iostream>
#include <fstream>

class NetPred
{
public:
    NetPred();
    NetPred(const NetPred& orig) = delete;
    NetPred &operator= (const NetPred&) = delete;
    ~NetPred();

    void Clear();

    void UpdateSizeAndDelay(uint32_t size, uint32_t encoded_size, double delay_in_ms);
    uint32_t GetNextFrameSize();

    void SetRecordedLen(int record_len);

    void SetTargetDelay(double target_in_ms);

    double GetTargetDelay();

    void SetMaxTargetSize(uint32_t maxBytes);
    void SetMinTargetSize(uint32_t minBytes);

    void SetFPS(double fps);

    // factor 0.0 ~ 1.0. 0.0 means reacting fastest, but may has spikes. 1.0 means reacting slowest, but most smoothing. 0.5 by default
    void SetOutputFilterFactor(double factor);
protected:
    int CurrentState();
    void UpdateModel();

    void CheckNewSteadyState(uint32_t encoded_size, double delay_in_ms);
    void AdjustTarget(double delay_in_ms);

    void UpdateModelNormal(std::deque<double>& m_delays, std::deque<double>& m_sizes);
    void UpdateModelSmall(std::deque<double>& m_delays, std::deque<double>& m_sizes);
    void UpdateModelSafe(std::deque<double>& m_delays, std::deque<double>& m_sizes);

    double WeightedMean(std::deque<double>& data);

    bool SanityCheck();
    bool IsSpikeOn();
    bool OscillationDetected();
    bool UpdateSteadyState();
    bool ManageRecoveryAttempt();

    double m_targetDelay;
    int m_recordedLen;
    double m_nextTargetSize;

    double m_exceptionThreshold;

    double m_reverseBandWidth;  //  in Mbytes/per sec
    double m_propagotionDelay;  // in ms ( 10^(-3))

    double m_forgotRatio;

    std::deque<double> m_delays;
    std::deque<double> m_sizes;

    std::deque<double> m_effectiveDelays;
    std::deque<double> m_effectiveSizes;
    double m_effectiveSizeThreshold;
    uint32_t m_effectiveDataLen;

    bool m_dumpflag;
    std::ofstream m_dumpfile;
    std::ofstream m_dumpPoints;

    double m_maxTargetSize;
    double m_minTargetSize;

    // Settings for spike-checking
    double m_fps;
    double m_estimatedThresholdSize;
    uint32_t m_timeout;       // seconds to consider if behaviour is steady.
    uint32_t m_timeToExplore; // seconds before attempting recovery. Initialized to m_timeout
    double m_estimateAcc;
    uint32_t m_observeCounter;
    uint32_t m_observeCounterThreshold;

    // output filter, IIR
    double m_filteredTargetSize;
    double m_filterFactor;

    // Limit excessively high estimate of bandwidth
    // Below corresponds to 400Mbps
    const double MinReverseBandwidth = 0.02;
    const double SubstantialChangeThreshold = 0.1;

    double m_previousTargetSize = 0;

    // Spike-checking and recovery state
    bool m_enableSteadyStateCheck;
    int  m_framesSinceLastSpike;
    int  m_encFramesForThreshold;
    bool m_newState;
    int  m_spikes;
    bool m_recoveryAttempt;
    int  m_recoveryFrames;
};

#endif
