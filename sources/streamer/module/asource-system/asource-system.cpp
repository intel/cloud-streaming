// Copyright (C) 2022-2023 Intel Corporation

/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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

#include <stdio.h>

#include <thread>

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "asource.h"

#ifdef ENABLE_AUDIO

#include "ga-win32-wasapi.h"

static int asource_initialized = 0;
static int asource_started = 0;
static std::thread asource_th;

static struct ga_wasapi_param audioparam;

static int
asource_init(void *arg, void(*p)(struct timeval))
{
    int delay = 0;
    struct RTSPConf *rtspconf = rtspconf_global();
    if(asource_initialized != 0)
        return 0;
    //
    if((delay = ga_conf_readint("audio-init-delay")) > 0) {
        usleep(delay*1000);
    }
    //
    audioparam.channels = rtspconf->audio_channels;
    audioparam.samplerate = rtspconf->audio_samplerate;
    if(rtspconf->audio_device_format == ga_sample_format::GA_SAMPLE_FMT_S16) {
        audioparam.bits_per_sample = 16;
    } else {
        ga_logger(Severity::ERR, "audio source: unsupported audio format (%d).\n",
            rtspconf->audio_device_format);
        return -1;
    }
    if(rtspconf->audio_device_channel_layout != ga_audio_layout::GA_CH_LAYOUT_STEREO) {
        ga_logger(Severity::ERR, "audio source: unsupported channel layout (%llu).\n",
            rtspconf->audio_device_channel_layout);
        return -1;
    }
    if(ga_wasapi_init(&audioparam) < 0) {
        ga_logger(Severity::ERR, "WASAPI: initialization failed.\n");
        return -1;
    }
    if(audio_source_setup(audioparam.chunk_size, audioparam.samplerate, audioparam.bits_per_sample, audioparam.channels) < 0) {
        ga_logger(Severity::ERR, "audio source: setup failed.\n");
        ga_wasapi_close(&audioparam);
        return -1;
    }
    asource_initialized = 1;
    ga_logger(Severity::INFO, "audio source: setup chunk=%d, samplerate=%d, bps=%d, channels=%d\n",
        audioparam.chunk_size,
        audioparam.samplerate,
        audioparam.bits_per_sample,
        audioparam.channels);
    return 0;
}

static void
asource_threadproc() {
    int r;
    unsigned char *fbuffer = NULL;
    //
    if(asource_init(NULL, NULL) < 0) {
        exit(-1);
    }
    if((fbuffer = (unsigned char*) malloc(audioparam.chunk_bytes)) == NULL) {
        ga_logger(Severity::ERR, "audio source: malloc failed %d bytes\n", audioparam.chunk_bytes);
        exit(-1);
    }
    //
    ga_logger(Severity::INFO, "audio source thread started: tid=%ld\n", ga_gettid());
    //
    while(asource_started != 0) {
        r = ga_wasapi_read(&audioparam, fbuffer, audioparam.chunk_size);
        if(r < 0) {
            ga_logger(Severity::ERR, "audio source: WASAPI read failed.\n");
            break;
        }
        audio_source_buffer_fill(fbuffer, r);
    }
    //
    if(fbuffer)
        free(fbuffer);
    ga_logger(Severity::INFO, "audio capture thread terminated.\n");
}

static int
asource_deinit(void *arg) {
    ga_wasapi_close(&audioparam);
    asource_initialized = 0;
    return 0;
}

static int
asource_start(void *arg) {
    if(asource_started != 0)
        return 0;
    asource_started = 1;
    asource_th = std::thread(asource_threadproc);
    return 0;
}

static int
asource_stop(void *arg) {
    if(asource_started == 0)
        return 0;
    asource_started = 0;
    asource_th.join();
    return 0;
}

ga_module_t *
module_load() {
    static ga_module_t m;
    bzero(&m, sizeof(m));
    m.type = GA_MODULE_TYPE_ASOURCE;
    m.name = "asource-system";
    m.init = asource_init;
    m.start = asource_start;
    //m.threadproc = asource_threadproc;
    m.stop = asource_stop;
    m.deinit = asource_deinit;
    return &m;
}

#endif    /* ENABLE_AUDIO */
