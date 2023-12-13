// Copyright (C) 2022 Intel Corporation

/*
 * Copyright (c) 2013 Chun-Ying Huang
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "asource.h"

#include "ga-win32-wasapi.h"

#define REFTIMES_PER_SEC       10000000  // 1s, in 100-ns unit
#define REFTIMES_PER_MILLISEC  10000
#define REQUESTED_DURATION     100000    // 200ms, in 100-ns unit
// XXX: Small REQUESTED_DURATION sometimes may cause problems ..

#define GA_WASAPI_TRACE_HRESULT(result, message) \
    ga_logger(Severity::ERR, "wasapi: " __FUNCTION__ ": %s, result = 0x%08x\n", message, result)

#define GA_WASAPI_CHECK_HRESULT(result, message, retval) \
    if (FAILED(result)) { \
        GA_WASAPI_TRACE_HRESULT(result, message); \
        return retval; \
    }

#define GA_WASAPI_CHECK_PTR(ptr, message, retval) \
    if (!ptr) { \
        GA_WASAPI_TRACE_HRESULT(E_POINTER, message); \
        return retval; \
    }

#define GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, result, message, retval) \
    if (FAILED(result)) { \
        GA_WASAPI_TRACE_HRESULT(result, message); \
        ga_wasapi_release(wasapi); \
        return retval; \
    }

#define GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, ptr, message, retval) \
    if (!ptr) { \
        GA_WASAPI_TRACE_HRESULT(E_POINTER, message); \
        ga_wasapi_release(wasapi); \
        return retval; \
    }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

static inline void ga_wasapi_release(ga_wasapi_param* wasapi) {
    if (wasapi) {
        CoTaskMemFree(wasapi->pwfx);
        wasapi->pwfx = nullptr;

        if (wasapi->pEnumerator) {
            wasapi->pEnumerator->Release();
            wasapi->pEnumerator = nullptr;
        }
        if (wasapi->pDevice) {
            wasapi->pDevice->Release();
            wasapi->pDevice = nullptr;
        }
        if (wasapi->pAudioClient) {
            wasapi->pAudioClient->Release();
            wasapi->pAudioClient = nullptr;
        }
        if (wasapi->pCaptureClient) {
            wasapi->pCaptureClient->Release();
            wasapi->pCaptureClient = nullptr;
        }

        CoUninitialize();
    }
}

static HRESULT check_wave_format(ga_wasapi_param* wparam) {
    GA_WASAPI_CHECK_PTR(wparam, "Invalid argument", E_INVALIDARG);
    GA_WASAPI_CHECK_PTR(wparam->pwfx, "Invalid argument", E_INVALIDARG);

    WAVEFORMATEX* pwfx = wparam->pwfx;
    WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*) wparam->pwfx;

    HRESULT res = S_OK;

    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            wparam->isFloat = 1;
        } else if (ext->SubFormat != KSDATAFORMAT_SUBTYPE_PCM) {
            res = E_UNSUPPORTED_TYPE;
        }
    } else if (pwfx->wFormatTag != WAVE_FORMAT_PCM) {
        res = E_UNSUPPORTED_TYPE;
    }
    GA_WASAPI_CHECK_HRESULT(res, "non-PCM audio format is not supported", res);

    ga_logger(Severity::INFO, "wasapi: num channels = %d\n", pwfx->nChannels);
    if (pwfx->nChannels != 2) {
        res = E_UNSUPPORTED_TYPE;
    }
    GA_WASAPI_CHECK_HRESULT(res, "num channels != 2 is not supported", res);

    ga_logger(Severity::INFO, "wasapi: sample rate = %d, bits per sample = %d\n", pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
    ga_logger(Severity::INFO, "rtsp: sample rate = %d, bits per sample = %d\n", wparam->samplerate, wparam->bits_per_sample);

    if (wparam->samplerate != pwfx->nSamplesPerSec) {
        res = E_UNSUPPORTED_TYPE;
    }
    GA_WASAPI_CHECK_HRESULT(res, "wasapi: audio sample rate mismatch", res);

    if (wparam->isFloat) {
        if (wparam->bits_per_sample != 16) {
            res = E_UNSUPPORTED_TYPE;
        }
    } else if (wparam->bits_per_sample != pwfx->wBitsPerSample) {
        res = E_UNSUPPORTED_TYPE;
    }
    GA_WASAPI_CHECK_HRESULT(res, "wasapi: audio bits per sample mismatch", res);

    return 0;
}

int ga_wasapi_init(ga_wasapi_param* wasapi) {
    ga_logger(Severity::INFO, "wasapi: audio capture init started\n");

    GA_WASAPI_CHECK_PTR(wasapi, "Invalid argument", -1);

    HRESULT res = S_OK;

    res = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (res == E_FAIL) {
        GA_WASAPI_TRACE_HRESULT(res, "CoInitializeEx already initialized");
        CoUninitialize();
    }
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "CoInitializeEx failed", -1);

    res = CoCreateInstance(
            CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator,
            reinterpret_cast<void**>(&wasapi->pEnumerator));
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "CoCreateInstance failed", -1);
    GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, wasapi->pEnumerator, "IMMDeviceEnumerator object is null", -1);

    res = wasapi->pEnumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, &wasapi->pDevice);
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IMMDeviceEnumerator->GetDefaultAudioEndpoint failed", -1);
    GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, wasapi->pDevice, "DefaultAudioEndpoint object is null", -1);

    res = wasapi->pDevice->Activate(
            IID_IAudioClient, CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(&wasapi->pAudioClient));
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IMMDevice->Activate failed", -1);
    GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, wasapi->pAudioClient, "IAudioClient object is null", -1);

    res = wasapi->pAudioClient->GetMixFormat(&wasapi->pwfx);
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IAudioClient->GetMixFormat failed", -1);
    GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, wasapi->pwfx, "MixFormat object is null", -1);

    // XXX: check pwfx against the audio configuration
    res = check_wave_format(wasapi);
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "audio format is not supported", -1);

    REFERENCE_TIME hnsRequestedDuration = REQUESTED_DURATION;
    ga_logger(Severity::INFO, "Target to set the audio capture duration = %d ms\n", hnsRequestedDuration / 10000);

    res = wasapi->pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK/*0*/,
            hnsRequestedDuration,
            0,
            wasapi->pwfx,
            nullptr);
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IAudioClient->Initialize failed", -1);

    // Get the size of the allocated buffer.
    res = wasapi->pAudioClient->GetBufferSize(&wasapi->bufferFrameCount);
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IAudioClient->GetBufferSize failed", -1);

    wasapi->hnsActualDuration = (UINT32) ((double) REFTIMES_PER_SEC *
        wasapi->bufferFrameCount / wasapi->pwfx->nSamplesPerSec);
    wasapi->bufferFillInt = (DWORD) (wasapi->hnsActualDuration / REFTIMES_PER_MILLISEC / 2);

    ga_logger(Severity::INFO, "Actual audio capture duration = %d ms\n", wasapi->hnsActualDuration/10000);

    res = wasapi->pAudioClient->GetService(
            IID_IAudioCaptureClient,
            reinterpret_cast<void**>(&wasapi->pCaptureClient));
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IAudioClient->GetService failed", -1);
    GA_WASAPI_CHECK_PTR_CLEANUP(wasapi, wasapi->pCaptureClient, "IAudioCaptureClient object is null", -1);

    // sync configurations with other platforms
    wasapi->chunk_size = (wasapi->bufferFrameCount)/2;
    wasapi->bits_per_frame = wasapi->bits_per_sample * wasapi->channels;
    wasapi->chunk_bytes = wasapi->chunk_size * wasapi->bits_per_frame;

    // start capture
    res = wasapi->pAudioClient->Start();
    GA_WASAPI_CHECK_HRESULT_CLEANUP(wasapi, res, "IAudioClient->Start failed", -1);

    gettimeofday(&wasapi->initialTimestamp, NULL);

    ga_logger(Severity::INFO, "wasapi: audio capture init succeeded\n");

    return 0;
}

int ga_wasapi_read(ga_wasapi_param* wasapi, unsigned char* wbuf, int wframes) {
    GA_WASAPI_CHECK_PTR(wasapi, "Invalid argument", -1);

    int i, copysize = 0, copyframe = 0;
    HRESULT res = S_OK;
    UINT32 packetLength, numFramesAvailable;
    BYTE* pData;
    DWORD flags;
    UINT64 framePos;
    int srcunit = wasapi->bits_per_sample / 8;
    int dstunit = audio_source_bitspersample() / 8;
    struct timeval afterSleep;
    bool filldata = false;
    // frame statistics
    struct timeval currtv;

    if(wasapi->firstRead.tv_sec == 0) {
        gettimeofday(&wasapi->firstRead, nullptr);
        wasapi->trimmedFrames = (UINT64) (1.0 * wasapi->samplerate *
            tvdiff_us(&wasapi->firstRead, &wasapi->initialTimestamp) /
            1000000);
        wasapi->silenceFrom = wasapi->firstRead;
        ga_logger(Severity::INFO, "wasapi: estimated trimmed frames = %lld\n",
            wasapi->trimmedFrames);
    }

    gettimeofday(&currtv, nullptr);
    if (wasapi->lastTv.tv_sec == 0) {
        gettimeofday(&wasapi->lastTv, nullptr);
        wasapi->frames = 0;
        wasapi->sframes = 0;
        wasapi->slept = 0;
    } else if (tvdiff_us(&currtv, &wasapi->lastTv) >= 1000000) {
#if 0
        ga_logger(Severity::INFO,
            "Frame statistics: s=%d, ns=%d, sum=%d (sleep=%d)\n",
            wasapi->sframes, wasapi->frames,
            wasapi->sframes + wasapi->frames,
            wasapi->slept);
#endif
        wasapi->lastTv = currtv;
        wasapi->frames = wasapi->sframes = wasapi->slept = 0;
    }

    if (wasapi->fillSilence > 0) {
        if (wasapi->fillSilence <= wframes) {
            copyframe = (int) wasapi->fillSilence;
        } else {
            copyframe = wframes;
        }
        copysize = copyframe * wasapi->channels * dstunit;
        ZeroMemory(wbuf, copysize);

        wasapi->fillSilence -= copyframe;
        wframes -= copyframe;
        wasapi->sframes += copyframe;
        if (wframes <= 0) {
            return copyframe;
        }
    }

    GA_WASAPI_CHECK_PTR(wasapi->pCaptureClient, "IAudioCaptureClient object is null", -1);

    res = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
    GA_WASAPI_CHECK_HRESULT(res, "IAudioCaptureClient->GetNextPacketSize failed", -1);

    if (packetLength == 0) {
        Sleep(wasapi->bufferFillInt);
        gettimeofday(&afterSleep, NULL);
        wasapi->slept++;

        res = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
        GA_WASAPI_CHECK_HRESULT(res, "IAudioCaptureClient->GetNextPacketSize failed", -1);

        if (packetLength == 0) {
            // fill silence
            double silenceFrame = 1.0 *
                tvdiff_us(&afterSleep, &wasapi->silenceFrom) *
                wasapi->samplerate / 1000000.0;
            wasapi->fillSilence += (UINT64) silenceFrame;
            wasapi->silenceFrom = afterSleep;
        }
    }

    while (packetLength != 0 && wframes >= (int) packetLength) {
        res = wasapi->pCaptureClient->GetBuffer(&pData,
            &numFramesAvailable, &flags, &framePos, nullptr);
        GA_WASAPI_CHECK_HRESULT(res, "IAudioCaptureClient->GetBuffer failed", -1);

        if (packetLength != numFramesAvailable) {
            ga_logger(Severity::WARNING, "WARNING: packetLength(%d) != numFramesAvailable(%d)\n",
                packetLength, numFramesAvailable);
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            wasapi->sframes += numFramesAvailable;
            ZeroMemory(&wbuf[copysize], numFramesAvailable * wasapi->channels * dstunit);
            //ga_logger(Severity::INFO, "WASAPI-DEBUG: write slience (%d).\n", numFramesAvailable);
        } else {
            wasapi->frames += numFramesAvailable;
            if (wasapi->isFloat) {
                float* r = (float*) (pData);
                short* w = (short*) (&wbuf[copysize]);
                int cc = numFramesAvailable * wasapi->channels;
                for(i = 0; i < cc; i++) {
                    *w = (short) (*r * 32768.0);
                    r++;
                    w++;
                }
            } else {
                CopyMemory(&wbuf[copysize], pData, numFramesAvailable * wasapi->channels * dstunit);
            }
            //ga_logger(Severity::INFO, "WASAPI-DEBUG: write data (%d).\n", numFramesAvailable);
        }

        wframes -= numFramesAvailable;
        copyframe += numFramesAvailable;
        copysize += numFramesAvailable * wasapi->channels * dstunit;

        res = wasapi->pCaptureClient->ReleaseBuffer(numFramesAvailable);
        GA_WASAPI_CHECK_HRESULT(res, "IAudioCaptureClient->ReleaseBuffer failed", -1);

        res = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
        GA_WASAPI_CHECK_HRESULT(res, "IAudioCaptureClient->GetNextPacketSize failed", -1);

        filldata = true;
    }

    if (filldata) {
        gettimeofday(&wasapi->silenceFrom, nullptr);
    }

    return copyframe;
}

int ga_wasapi_close(ga_wasapi_param* wasapi) {
    if (wasapi) {
        if (wasapi->pAudioClient) {
            HRESULT res = wasapi->pAudioClient->Stop();
            GA_WASAPI_TRACE_HRESULT(res, "IAudioCaptureClient->Stop failed");
        }
    }

    ga_wasapi_release(wasapi);

    return 0;
}
