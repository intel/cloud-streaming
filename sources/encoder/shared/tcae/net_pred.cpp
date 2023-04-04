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

#include <stdio.h>
#include "net_pred.h"
#include <stdint.h>
#include <random>
#include <chrono>
#include <algorithm>
#include <math.h>
#include <fstream>
#include <cmath>

NetPred::NetPred():
    m_targetDelay(16),
    m_recordedLen(100),
    m_nextTargetSize(0.0),
    m_exceptionThreshold(1.0),
    m_reverseBandWidth(1.0),
    m_propagotionDelay(0.0),
    m_effectiveSizeThreshold(1.0),
    m_effectiveDataLen(2),
#ifdef _DEBUG
    m_dumpflag(true),
#else
    m_dumpflag(false),
#endif
    m_maxTargetSize(1000.0),
    m_minTargetSize(5.0),
    m_fps(30.0),
    m_estimatedThresholdSize(0.0),
    m_timeout(10),
    m_timeToExplore(m_timeout),
    m_estimateAcc(0.0),
    m_observeCounter(0),
    m_observeCounterThreshold(5),
    m_filteredTargetSize(0.0),
    m_filterFactor(0.5),
    m_enableSteadyStateCheck(true),
    m_framesSinceLastSpike(0),
    m_encFramesForThreshold(0),
    m_newState(false),
    m_spikes(0),
    m_recoveryAttempt(false),
    m_recoveryFrames(0)
{

    m_forgotRatio = pow(0.01, 1.0 / 2 /m_recordedLen);

    //Enable steady state check
    char* envStr = nullptr;

    envStr = getenv("TCAE_STEADY_STATE_CHECK");
    if (envStr){
        m_enableSteadyStateCheck = (atoi(envStr) ? true: false);
    }

    envStr = getenv("TCAE_NETPRED_DUMPS");
    if (envStr){
        m_dumpflag = (atoi(envStr) ? true: false);
    }

    if (m_dumpflag)
    {
        m_dumpfile.open("/tmp/netpred2.0_dump.csv");
        m_dumpPoints.open("/tmp/netpred2.0_points.log");
        if (m_dumpfile.good()) {
            m_dumpfile << "encoded_size, m_observeCounter, m_framesSinceLastSpike, m_encFramesSinceLastSpike, m_newState, m_spikes, m_recoveryAttempt, m_recoveryFrames, m_estimatedAcc, m_estimatedThresholdSize, m_timeToExplore, sizeInK, delay_in_ms, m_reverseBandWidth, m_propagotionDelay, standardError, propagotionDelay, m_targetDelay, m_nextTargetSize, m_networklimitor, reverseBandwidth" << std::endl;
        }
    }
}

NetPred::~NetPred()
{

    if (m_dumpflag)
    {
        m_dumpfile.close();
        m_dumpPoints.close();
    }
}

void NetPred::Clear()
{

    m_sizes.clear();
    m_delays.clear();
}

void NetPred::UpdateSizeAndDelay(uint32_t size, uint32_t encoded_size, double delay_in_ms)
{

    CheckNewSteadyState(encoded_size, delay_in_ms);

    if (m_dumpfile.good())
    {
        m_dumpfile << encoded_size << "," <<  m_observeCounter << "," <<  m_framesSinceLastSpike << "," << m_encFramesForThreshold << "," <<  m_newState << "," << m_spikes << "," << m_recoveryAttempt << "," << m_recoveryFrames << "," << m_estimateAcc << "," <<  m_estimatedThresholdSize << "," << m_timeToExplore << ",";
    }

    bool inputValid = true;
    if (delay_in_ms < 0)
    {
        inputValid = false;
    }

    if (!inputValid) // invalid input
    {
        m_nextTargetSize = m_nextTargetSize * 0.95;
        if (m_nextTargetSize < m_minTargetSize)
        {
            m_nextTargetSize = m_minTargetSize;
        }

        if (m_dumpfile.good())
        {
            m_dumpfile << (double)size / 1000.0 << ", " << delay_in_ms << ", " << m_reverseBandWidth << ", " << m_propagotionDelay << ", " << 0 << ", " << m_propagotionDelay << ", "
                << m_targetDelay << ", " << m_nextTargetSize << ", " << m_estimatedThresholdSize << ", " << m_reverseBandWidth << std::endl;
        }
        return;
    }

    if (m_dumpPoints.good())
    {
        m_dumpPoints << "NewFrame ---------------------------------" << std::endl;
        m_dumpPoints << "Size, Delay" << std::endl;
    }

    double sizeInK = (double)size / 1000.0;

    m_delays.push_front(delay_in_ms);
    m_sizes.push_front(sizeInK);

    if (int(m_delays.size()) > m_recordedLen)
    {
        m_delays.pop_back();
        m_sizes.pop_back();
    }

    if (sizeInK >= m_effectiveSizeThreshold)
    {
        m_effectiveSizes.push_front(sizeInK);
        m_effectiveDelays.push_front(delay_in_ms);
        if (m_effectiveDelays.size() > m_effectiveDataLen)
        {
            m_effectiveSizes.pop_back();
            m_effectiveDelays.pop_back();
        }
    }

    UpdateModel();
    double propagotionDelay = m_propagotionDelay;
    double reverseBandWidth = m_reverseBandWidth;
    double standardError = 0.0;

    if (m_sizes.size() >= 0.2 * m_recordedLen)
    {
        double mse = 0.0;
        double count = 0.0;
        double weight = 1.0;
        for (size_t i = 1; i < m_sizes.size(); i++)
        {
            if (m_delays[i] < 1e-6 && m_sizes[i] < 1e-6)
            {
                ;
            }
            else
            {
                double estDelay = m_reverseBandWidth * m_sizes[i] + m_propagotionDelay;
                mse += weight * weight * (m_delays[i] - estDelay) * (m_delays[i] - estDelay);
                count += weight * weight;
            }
            weight *= m_forgotRatio;
        }

        //initially mse = 0 and count = 0
        //they are initialized in the same place in cycle
        //so valid value can be obtained only in case when mse & count != 0
        if (count > 1e-6)
        {
            mse = mse / count;

            standardError = sqrt(mse);

            double rtEstDelay = m_reverseBandWidth * sizeInK + m_propagotionDelay;

            if (delay_in_ms - rtEstDelay > m_exceptionThreshold * standardError)
            {
                propagotionDelay = delay_in_ms - m_reverseBandWidth * sizeInK;
                if (delay_in_ms > 0.9 * m_targetDelay)
                {
                    propagotionDelay = 0.5 * propagotionDelay + 0.5 * m_propagotionDelay;
                    reverseBandWidth = (delay_in_ms - propagotionDelay) / sizeInK;
                }

                if (m_dumpPoints.good()) {
                    m_dumpPoints << "adj_reverseBandwidth = " << reverseBandWidth << std::endl;
                    m_dumpPoints << "adj_propagotionDelay = " << propagotionDelay << std::endl;
                }

            }
        }
    }

    m_previousTargetSize = m_nextTargetSize;

    m_nextTargetSize = (0.9 * m_targetDelay - propagotionDelay) / reverseBandWidth;

    AdjustTarget(delay_in_ms);

    if ((m_nextTargetSize < m_minTargetSize) || isnan(m_nextTargetSize))
    {
        m_nextTargetSize = m_minTargetSize;
    }

    if (m_nextTargetSize > m_maxTargetSize)
    {
        m_nextTargetSize = m_maxTargetSize;
    }

    if (m_filteredTargetSize < 1.0)
    {
        m_filteredTargetSize = m_nextTargetSize;
    }

    m_filteredTargetSize = m_filteredTargetSize * 0.9 * m_filterFactor + m_nextTargetSize * (1 - m_filterFactor * 0.9);
    m_nextTargetSize = m_filteredTargetSize;


    // dump
    if (m_dumpfile.good())
    {
        m_dumpfile << sizeInK << ", " << delay_in_ms << ", " << m_reverseBandWidth << ", " << m_propagotionDelay << ", " << m_exceptionThreshold * standardError << ", " << propagotionDelay << ", "
            << m_targetDelay << ", " << m_nextTargetSize << ", " << m_estimatedThresholdSize << ", " << reverseBandWidth << std::endl;
    }

    return;
}

void NetPred::UpdateModel()
{

    std::deque<double> delays;
    std::deque<double> sizes;
    delays.clear();
    sizes.clear();
    bool validSequence = false;
    for (size_t i = 0; i < m_sizes.size(); i++)
    {
        delays.push_back(m_delays[i]);
        sizes.push_back(m_sizes[i]);
        if (m_sizes[i] >= m_effectiveSizeThreshold)
        {
            validSequence = true;
        }
    }

    if (!validSequence)
    {
        // original code fron NetPred.cpp
        //for (int i = m_effectiveSizes.size() - 1; i >= 0; i--)
        //{
        //    delays.push_front(m_effectiveDelays[i]);
        //    delays.pop_back();
        //    sizes.push_front(m_effectiveSizes[i]);
        //    sizes.pop_back();
        //}
        // rewrite to reverse interatore to avoid type cast  size_t -> long long -> see i >= 0 
        for (auto it = m_effectiveSizes.crbegin(); it != m_effectiveSizes.crend(); it++)
        {
            delays.push_front(*it);
            delays.pop_back();
            sizes.push_front(*it);
            sizes.pop_back();
        }

    }

    UpdateModelNormal(delays, sizes);
    if (!SanityCheck())
    {
        if (m_dumpPoints.good())
            m_dumpPoints << "UpdateModelSafe ......" << std::endl;
        UpdateModelSafe(delays, sizes);
    }

    if (!SanityCheck())
    {
        if (m_dumpPoints.good())
            m_dumpPoints << "UpdateModelSmall ......" << std::endl;
        UpdateModelSmall(delays, sizes);
    }

    //double mse = 0.0;
}

void NetPred::UpdateModelNormal(std::deque<double>& delays, std::deque<double>& sizes)
{

    if (delays.size() < 0.2 * m_recordedLen)
    {
        if (m_dumpPoints.good())
            m_dumpPoints << "UpdateModelSmall ......" << std::endl;
        return UpdateModelSmall(delays, sizes);
    }

    double meanDelay = WeightedMean(delays);
    double meanSize = WeightedMean(sizes);

    if (m_dumpPoints.good())
        m_dumpPoints << "MeanSize, MeanDelay: " << meanSize << "," << meanDelay << std::endl;

    double accD = 0.0;
    double accN = 0.0;
    double weight = 1.0;
    for (size_t i = 0; i < delays.size(); i++)
    {
        if (delays[i] < 1e-6 && sizes[i] < 1e-6)
        {
            ;
        }
        else
        {
            accD += weight * weight * (delays[i] - meanDelay) * (sizes[i] - meanSize);
            accN += weight * weight * (sizes[i] - meanSize) * (sizes[i] - meanSize);
        }

        weight *= m_forgotRatio;

        if (m_dumpPoints.good())
            m_dumpPoints << sizes[i] << "," << delays[i] << std::endl;
    }

    if (accN < 1e-6)
    {
        if (m_dumpPoints.good())
            m_dumpPoints << "UpdateModelSmall ......" << std::endl;

        return UpdateModelSmall(delays, sizes);
    }
    else
    {
        m_reverseBandWidth = accD / accN;
        m_propagotionDelay = meanDelay - m_reverseBandWidth * meanSize;
    }

    if (m_dumpPoints.good()) {
        m_dumpPoints << "m_reverseBandwidth = " << m_reverseBandWidth << std::endl;
        m_dumpPoints << "m_propagotionDelay = " << m_propagotionDelay << std::endl;
    }

}

void NetPred::UpdateModelSmall(std::deque<double>& delays, std::deque<double>& sizes)
{

    double meanDelay = WeightedMean(delays);
    double meanSize = WeightedMean(sizes);

    if (m_dumpPoints.good())
        m_dumpPoints << "MeanSize, MeanDelay: " << meanSize << "," << meanDelay << std::endl;

    if (meanSize < 1e-6 || meanDelay < 1e-6)
    {
        // invalid data set, leave the model unchanged
        return;
    }

    m_reverseBandWidth = meanDelay / meanSize;
    m_propagotionDelay = 0.1;

    if (m_dumpPoints.good()) {
        m_dumpPoints << "m_reverseBandwidth = " << m_reverseBandWidth << std::endl;
        m_dumpPoints << "m_propagotionDelay = " << m_propagotionDelay << std::endl;
    }
}

void NetPred::UpdateModelSafe(std::deque<double>& delays, std::deque<double>& sizes)
{

    double meanDelay = WeightedMean(delays);
    double meanSize = WeightedMean(sizes);

    std::deque<double> safeDelays;
    std::deque<double> safeSizes;

    for (size_t i = 0; i < delays.size(); i ++)
    {
        double deltaX = delays[i] - meanDelay;
        double deltaY = sizes[i] - meanSize;
        if (deltaX * deltaY > 0)
        {
            safeDelays.push_back(delays[i]);
            safeSizes.push_back(sizes[i]);
        }
        else
        {
            safeDelays.push_back(0.0);
            safeSizes.push_back(0.0);
        }
    }

    UpdateModelNormal(safeDelays, safeSizes);
}

double NetPred::WeightedMean(std::deque<double>& data)
{

    double accD = 0.0;
    double accN = 0.0;
    double weight = 1.0;
    for (auto ite = data.begin(); ite != data.end(); ite++)
    {
        accD += weight * (*ite);
        accN += weight;
        weight *= m_forgotRatio;
    }
    if (accN < 1e-6)
    {
        return 0.0;
    }
    return accD / accN;

}

bool NetPred::OscillationDetected()
{
    //Multiple Spikes identified in short duration (m_spikes > 1)
    //Hints at oscillation pattern around a different steady state
    return ((m_framesSinceLastSpike < m_timeout * m_fps) && m_spikes >= 2);
}

bool NetPred::UpdateSteadyState()
{
    //Checks if the frame size corresponding to the estimated "steady state"
    //bitrate needs to be updated and acts on it. Returns if this value has
    //been updated.

    // Update average framesize taking into account accumulated frame sizes over
    // all spikes in current evaluation period

    bool updateThreshold = false;
    if (m_newState && m_encFramesForThreshold > 0 && m_spikes >= 2)
    {
        double newThresholdSize = m_estimateAcc / m_encFramesForThreshold / 1000.0;

        double lowLimit  = (1 - SubstantialChangeThreshold) * m_estimatedThresholdSize;
        double highLimit = (1 + SubstantialChangeThreshold) * m_estimatedThresholdSize;

        updateThreshold = (newThresholdSize < lowLimit) || (newThresholdSize > highLimit);
        if (updateThreshold)
            m_estimatedThresholdSize = newThresholdSize;
    }

    return updateThreshold;
}

void NetPred::CheckNewSteadyState(uint32_t encoded_size, double delay_in_ms)
{
    if (!m_enableSteadyStateCheck)
        return;

    bool spikeEnds = false;

    bool HighDelayInRecovery = m_recoveryAttempt && (delay_in_ms > m_targetDelay/2);

    if (delay_in_ms > m_targetDelay || delay_in_ms < 0.0 || HighDelayInRecovery)
    {
        ++m_observeCounter;
    }
    else
    {
        if (IsSpikeOn())
            spikeEnds = true;

        m_observeCounter = 0;
    }

    // Reset avg frame size estimation at end of spike
    if (spikeEnds)
    {
        //Book-keeping
        m_spikes++;
        if (OscillationDetected())
            m_newState = true;
        m_framesSinceLastSpike = 0;

        // Check and update Steady State Frame Size
        bool updateThreshold = UpdateSteadyState();

        // If 2 spikes occur during recovery, it is deemed failed to recover to original bitrate
        // Reset Recovery state for next attempt
        // If there is a change in the threshold value, reset recovery trigger time to original
        // Else increase recovery trigger time by 2x
        if (m_recoveryAttempt && m_spikes >= 2)
        {
            m_recoveryAttempt = false;
            m_recoveryFrames = 0;
            if (updateThreshold)
                m_timeToExplore = m_timeout;
            else
                m_timeToExplore *= 2;
        }
    }

    // Track bitstream size if there is an ongoing spike in window for checking
    // oscillatory behaviour or if we are in a recovery attempt
    if (IsSpikeOn() || m_recoveryAttempt)
    {
        m_estimateAcc += encoded_size;
        if (encoded_size != 0) {
            //Valid new frame encoded
            m_encFramesForThreshold++;
        }
    }

    //Increment frame counter since last spike
    m_framesSinceLastSpike++;

    //Manage recovery attempt to see if we can return to original settings
    ManageRecoveryAttempt();

    //Reset spike counter after timeout
    //m_newState is left unchanged till a successful recovery
    if (m_framesSinceLastSpike >= m_timeout * m_fps)
        m_spikes = 0;
}

void NetPred::AdjustTarget(double delay_in_ms)
{
    if (delay_in_ms >= 0.9 * m_targetDelay &&
        m_nextTargetSize >= m_previousTargetSize)
    {
        // High latency is observed, but model is not dropping frame size
        // This logic is meant to catch this situation
        m_nextTargetSize = m_previousTargetSize * 0.9;
    }

    if (!m_enableSteadyStateCheck || m_recoveryAttempt)
        return;

    //Cap to value below estimated steady state threshold size
    if (m_newState && m_estimatedThresholdSize > 0)
    {
        if (m_nextTargetSize > 0.95 * m_estimatedThresholdSize)
        {
            m_nextTargetSize = 0.95 * m_estimatedThresholdSize;
        }
    }
}

void NetPred::SetOutputFilterFactor(double factor)
{
    if (factor > 1.0)
       factor = 1.0;
    if (factor < 0.0)
        factor = 0.0;

    m_filterFactor = factor;
}

uint32_t NetPred::GetNextFrameSize()
{

    return (int)(m_nextTargetSize * 1000);
}

void NetPred::SetRecordedLen(int record_len)
{
    m_recordedLen = record_len;
}

void NetPred::SetTargetDelay(double target_in_ms)
{
    m_targetDelay = target_in_ms;
}

double NetPred::GetTargetDelay()
{

    return m_targetDelay;
}

void NetPred::SetMaxTargetSize(uint32_t maxBytes)
{
    m_maxTargetSize = (double)maxBytes / 1000.0;
}

void NetPred::SetMinTargetSize(uint32_t minBytes)
{
    m_minTargetSize = (double)minBytes / 1000.0;
}

void NetPred::SetFPS(double fps)
{
    m_fps = fps;
}

inline int NetPred::CurrentState()
{
    return 0;
}

inline bool NetPred::SanityCheck()
{
    return m_reverseBandWidth > MinReverseBandwidth && m_propagotionDelay >= 0.0;
}

inline  bool NetPred::IsSpikeOn()
{
    return (m_observeCounter >= m_observeCounterThreshold);
}

inline bool NetPred::ManageRecoveryAttempt()
{
    // This function manages recovery attempt after detecting an oscillation
    // Returns bool variable indicating if recovery is in progress

    bool isAttemptOngoing = m_recoveryAttempt && (m_recoveryFrames <= m_fps * m_timeout);

    bool initRecoveryCondition = (m_newState && !m_recoveryAttempt && !IsSpikeOn() &&
                                 (m_framesSinceLastSpike == m_timeToExplore* m_fps));

    // Return if no active attempt and no new attempt at recovery
    if (!initRecoveryCondition && !isAttemptOngoing) {
        m_recoveryAttempt = false;
        return false;
    }

    // Start recovery attempt if condition is met
    if (initRecoveryCondition) {
        m_recoveryAttempt = true;
        m_recoveryFrames = 0;
    }

    // If ongoing attempt, check for result & maintain state
    if (isAttemptOngoing)
    {
        m_recoveryFrames++;

        // If there is no latency spike for enough time, recovery is considered successful
        // Reset recovery state and remove flag indicating new state
        if (m_recoveryFrames == m_timeout * m_fps)
        {
            m_recoveryAttempt = false;
            m_recoveryFrames = 0;
            m_timeToExplore = m_timeout;

            m_newState = false;
            m_estimateAcc = 0.0;
            m_framesSinceLastSpike = 0;
            m_encFramesForThreshold = 0;
            m_estimatedThresholdSize = 0.0;
            m_spikes = 0;
        }
    }

    return m_recoveryAttempt;
}
