// Copyright (C) 2022 Intel Corporation

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

/**
 * @file
 * Interfaces for bridging encoders and sink servers: the implementation.
 */

#include <string.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <list>

#include "vsource.h"
#include "encoder-common.h"
#ifdef WIN32
#include "rtspconf.h"
#endif
using namespace std;

static std::shared_mutex encoder_lock;
static map<void*, void*> encoder_clients; /**< Count for encoder clients */

static bool threadLaunched = false;    /**< Encoder thread is running? */

// for pts sync between encoders
static std::mutex syncmutex;
static bool sync_reset = true;

// list of encoders
static ga_module_t *vencoder = NULL;    /**< Video encoder instance */
static ga_module_t *aencoder = NULL;    /**< Audio encoder instance */
static ga_module_t *sinkserver = NULL;    /**< Sink server instance */
static void *vencoder_param = NULL;    /**< Video encoder parameter */
static void *aencoder_param = NULL;    /**< Audio encoder parameter */

/**
 * Check if the encoder has been launched.
 *
 * @return 0 if encoder is not running or 1 if encdoer is running.
 */
int
encoder_running() {
    return threadLaunched ? 1 : 0;
}

/**
 * Register a video encoder module.
 *
 * @param m [in] Pointer to the video encoder module.
 * @param param [in] Pointer to the video encoder parameter.
 * @return Currently it always returns 0.
 *
 * The encoder module is launched when a client is connected.
 * The \a param is passed to the encdoer module when the module is launched.
 */
int
encoder_register_vencoder(ga_module_t *m, void *param) {
    if(vencoder != NULL) {
        ga_logger(Severity::WARNING, "encoder: warning - replace video encoder %s with %s\n",
            vencoder->name, m->name);
    }
    vencoder = m;
    vencoder_param = param;
    ga_logger(Severity::INFO, "video encoder: %s registered\n", m->name);
    return 0;
}

/**
 * Register an audio encoder module.
 *
 * @param m [in] Pointer to the audio encoder module.
 * @param param [in] Pointer to the audio encoder parameter.
 * @return Currently it always returns 0.
 *
 * The encoder module is launched when a client is connected.
 * The \a param is passed to the encdoer module when the module is launched.
 */
int
encoder_register_aencoder(ga_module_t *m, void *param) {
    if(aencoder != NULL) {
        ga_logger(Severity::WARNING, "encoder warning - replace audio encoder %s with %s\n",
            aencoder->name, m->name);
    }
    aencoder = m;
    aencoder_param = param;
    ga_logger(Severity::INFO, "audio encoder: %s registered\n", m->name);
    return 0;
}

/**
 * Register a sink server module.
 *
 * @param m [in] Pointer to the sink server module.
 * @return 0 on success, or -1 on error.
 *
 * The sink server is used to receive encoded packets.
 * It can then deliver the packets to clients or store the packets.
 *
 * A sink server MUST have implemented the \a send_packet interface.
 */
int
encoder_register_sinkserver(ga_module_t *m) {
    if(m->send_packet == NULL) {
        ga_logger(Severity::ERR, "encoder error: sink server %s does not define send_packet interface\n", m->name);
        return -1;
    }
    if(sinkserver != NULL) {
        ga_logger(Severity::WARNING, "encoder warning: replace sink server %s with %s\n",
            sinkserver->name, m->name);
    }
    sinkserver = m;
    ga_logger(Severity::INFO, "sink server: %s registered\n", m->name);
    return 0;
}

/**
 * Get the currently registered video encoder module.
 *
 * @return Pointer to the video encoder module, or NULL if not registered.
 */
ga_module_t *
encoder_get_vencoder() {
    return vencoder;
}

/**
 * Get the currently registered audio encoder module.
 *
 * @return Pointer to the audio encoder module, or NULL if not registered.
 */
ga_module_t *
encoder_get_aencoder() {
    return aencoder;
}

/**
 * Get the currently registered sink server module.
 *
 * @return Pointer to the sink server module, or NULL if not registered.
 */
ga_module_t *
encoder_get_sinkserver() {
    return sinkserver;
}

/**
 * Register an encoder client, and start encoder modules if necessary.
 *
 * @param rtsp [in] Pointer to the encoder client context.
 * @return 0 on success, or quit the program on error.
 *
 * The \a rtsp parameter is used to count the number of connected
 * encoder clients.
 * When the number of encoder clients changes from zero to a larger number,
 * all the encoder modules are started. When the number of encoder clients
 * becomes zero, all the encoder modules are stopped.
 * GamingAnywhere now supports only share-encoder model, so each encoder
 * module only has one instance, no matter how many clients are connected.
 *
 * Note that the number of encoder clients may be not equal to
 * the actual number of clients connected to the game server
 * It depends on how a sink server manages its clients.
 */
int
encoder_register_client(void /*RTSPContext*/ *rtsp) {
    std::unique_lock<std::shared_mutex> lock(encoder_lock);
    if(encoder_clients.size() == 0) {
        // initialize video encoder
        if(vencoder != NULL && vencoder->init != NULL) {
#ifdef WIN32
            if(vencoder->init(vencoder_param, NULL) < 0) {
                ga_logger(Severity::ERR, "video encoder: init failed.\n");
                exit(-1);;
            }
#else
            if(vencoder->init(vencoder_param) < 0) {
                ga_logger(Severity::ERR, "video encoder: init failed.\n");
                exit(-1);;
            }
#endif
        }
        // initialize audio encoder
        if(aencoder != NULL && aencoder->init != NULL) {
#ifdef WIN32
            if(aencoder->init(aencoder_param, NULL) < 0) {
                ga_logger(Severity::ERR, "audio encoder: init failed.\n");
                exit(-1);
            }
#else
            if(aencoder->init(aencoder_param) < 0) {
                ga_logger(Severity::ERR, "audio encoder: init failed.\n");
                exit(-1);
            }
#endif
        }
        // must be set before encoder starts!
        threadLaunched = true;
        // start video encoder
        if(vencoder != NULL && vencoder->start != NULL) {
            if(vencoder->start(vencoder_param) < 0) {
                ga_logger(Severity::ERR, "video encoder: start failed.\n");
                threadLaunched = false;
                exit(-1);
            }
        }
        // start audio encoder
        if(aencoder != NULL && aencoder->start != NULL) {
            if(aencoder->start(aencoder_param) < 0) {
                ga_logger(Severity::ERR, "audio encoder: start failed.\n");
                threadLaunched = false;
                exit(-1);
            }
        }
    }
    encoder_clients[rtsp] = rtsp;
    ga_logger(Severity::INFO, "encoder client registered: total %zu clients.\n", encoder_clients.size());
    return 0;
}

/**
 * Unregister an encoder client, and stop encoder modules if necessary.
 *
 * @param rtsp [in] Pointer to the encoder client context.
 * @return Currently it always returns 0.
 */
int
encoder_unregister_client(void /*RTSPContext*/ *rtsp) {
    std::unique_lock<std::shared_mutex> lock(encoder_lock);
    encoder_clients.erase(rtsp);
    ga_logger(Severity::INFO, "encoder client unregistered: %zu clients left.\n", encoder_clients.size());
    if(encoder_clients.size() == 0) {
        threadLaunched = false;
        ga_logger(Severity::INFO, "encoder: no more clients, quitting ...\n");
        if(vencoder != NULL && vencoder->stop != NULL)
            vencoder->stop(vencoder_param);
        if(vencoder != NULL && vencoder->deinit != NULL)
            vencoder->deinit(vencoder_param);
#ifdef ENABLE_AUDIO
        if(aencoder != NULL && aencoder->stop != NULL)
            aencoder->stop(aencoder_param);
        if(aencoder != NULL && aencoder->deinit != NULL)
            aencoder->deinit(aencoder_param);
#endif
        // reset sync pts
        std::lock_guard<std::mutex> lock(syncmutex);
        sync_reset = true;
    }
    return 0;
}

/**
 * Send a packet to a sink server.
 *
 * @param prefix [in] Name to identify the sender. Can be any valid string.
 * @param channelId [in] Channel id.
 * @param pkt [in] The packet to be delivery.
 * @param encoderPts [in] Encoder presentation timestamp in an integer.
 * @param ptv [in] Encoder presentation timestamp in \a timeval structure.
 * @return 0 on success, or -1 on error.
 *
 * \a channelId is used to identify whether this packet is an audio packet or
 * a video packet. A video packet usually uses a channel id ranges
 * from 0 to \a N-1, where \a N is the number of video tracks (usually 1).
 * A audio packet usually uses a channel id of \a N.
 */
int
encoder_send_packet(const char *prefix, int channelId, ga_packet_t *pkt, int64_t encoderPts, struct timeval *ptv) {
    if(sinkserver) {
        return sinkserver->send_packet(prefix, channelId, pkt, encoderPts, ptv);
    }
    ga_logger(Severity::ERR, "encoder: no sink server registered.\n");
    return -1;
}

#ifdef WIN32
/**
 * Send cursor info to a sink server.
 * @param cursorInfo [in] the cursor information.
 * @param ptv [in] timestamp the cursor is associated with
 * @return 0 on success, or -1 on error.
*/
int encoder_send_cursor(std::shared_ptr<CURSOR_DATA> cursorInfo, struct timeval *ptv) {
    if (sinkserver) {
        return sinkserver->send_cursor(cursorInfo, ptv);
    }
    return 0;
}
/**
 * Send cursor info to a sink server.
 * @param cursorInfo [in] the cursor information.
 * @param ptv [in] timestamp the cursor is associated with
 * @return 0 on success, or -1 on error.
*/
int encoder_send_qos(std::shared_ptr<QosInfo> qosInfo) {
    if (sinkserver) {
        return sinkserver->send_qos(qosInfo);
    }
    return 0;
}

/**
  * Get credit bytes from transport
  *
  * @ return credit bytes value
  */
int GetCreditBytes() {
    if (sinkserver) {
        ga_ioctl_credit_t credit_s;
        sinkserver->ioctl(GA_IOCTL_GET_CREDIT_BYTES, sizeof(ga_ioctl_credit_t), &credit_s);
        return credit_s.credit_bytes;
    }
    ga_logger(Severity::ERR, "encoder: no sink server registered\n");
    return -1;
}

/**
  * Send the bistream of bytes specified by sizeBits to client
  *
  * @param bitStream The pointer to bitstream
  * @param sizeBits the size of stream to be sent in bits
  * @return 0 on success. or -1 on error
  *
  */
int SendBitstream(const unsigned char*bitStream, unsigned int sizeBits) {
    ga_packet_t pkt{};
    struct timeval pkttv;
    gettimeofday(&pkttv, NULL);
    ga_init_packet(&pkt);
    pkt.data = (uint8_t*)bitStream;
    pkt.size = sizeBits / 8;
    return encoder_send_packet("video-encoder", 0, &pkt, pkt.pts, &pkttv);
}

#endif

