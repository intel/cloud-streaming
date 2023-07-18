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

/**
 * @file
 * Interfaces for bridging encoders and sink servers: the header.
 */

#ifndef __ENCODER_COMMON_H__
#define __ENCODER_COMMON_H__

#include "ga-common.h"
#include "ga-module.h"
#include "cursor.h"

typedef struct _FrameMetaData {
    bool last_slice;
    uint64_t capture_time_ms;
    uint64_t encode_start_ms;
    uint64_t encode_end_ms;
#ifdef E2ELATENCY_TELEMETRY_ENABLED
    uint16_t latency_msg_size;
    uint8_t *latency_msg_data;
#endif
}FrameMetaData;

typedef void (*qcallback_t)(int);

EXPORT int encoder_running();
EXPORT int encoder_register_vencoder(ga_module_t *m, void *param);
EXPORT int encoder_register_aencoder(ga_module_t *m, void *param);
EXPORT int encoder_register_sinkserver(ga_module_t *m);
EXPORT ga_module_t *encoder_get_vencoder();
EXPORT ga_module_t *encoder_get_aencoder();
EXPORT ga_module_t *encoder_get_sinkserver();
EXPORT int encoder_register_client(void *ctx);
EXPORT int encoder_unregister_client(void *ctx);

EXPORT int encoder_send_packet(const char *prefix, int channelId, ga_packet_t *pkt, int64_t encoderPts, struct timeval *ptv);
#ifdef WIN32
EXPORT int encoder_send_cursor(std::shared_ptr<CURSOR_DATA> cursorInfo,struct timeval *ptv);
EXPORT int encoder_send_qos(std::shared_ptr<QosInfo> qosInfo);
EXPORT int SendBitstream(const unsigned char*bitStream, unsigned int sizeBits);
EXPORT int GetCreditBytes();
#endif

#endif

