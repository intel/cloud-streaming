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

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "controller.h"
#include "encoder-common.h"
#include "QosMgt.h"

//#define    TEST_RECONFIGURE

// image source pipeline:
//    vsource -- [vsource-%d] --> filter -- [filter-%d] --> encoder

// configurations:
static char *imagepipefmt = (char *)"video-%d";
static char *filterpipefmt = (char*)"filter-%d";
static char *imagepipe0 = (char*)"video-0";
static char *filterpipe0 = (char*)"filter-0";
static char *filter_param[] = { imagepipefmt, filterpipefmt };
static char *video_encoder_param = imagepipefmt;//filterpipefmt;
static void *audio_encoder_param = NULL;

static struct gaRect *prect = NULL;
static struct gaRect rect;

static ga_module_t *m_vsource, *m_filter, *m_vencoder, *m_asource, *m_aencoder, *m_ctrl, *m_server;

void
handle_eventreport(struct timeval time) {
    int s = 0, err;

    ga_ioctl_clevent_t clevent;
    if (encoder_running() == 0) {
        return;
    }

    bzero(&clevent, sizeof(clevent));
    clevent.timeevent = time;

    if (m_vencoder->ioctl) {
        err = m_vencoder->ioctl(GA_IOCTL_UPDATE_CLIENT_EVENT, sizeof(clevent), &clevent);
        if (err < 0) {
            ga_logger(Severity::ERR, "update client event failed, err = %d.\n", err);
        }
    }
}
int
load_modules() {

    if((m_vencoder = ga_load_module("mod/desktop-capture", "vencoder_")) == NULL)
        return -1;
  if(ga_conf_readbool("enable-audio", 1) != 0) {
    //////////////////////////
#ifndef __APPLE__
    if((m_asource = ga_load_module("mod/asource-system", "asource_")) == NULL)
      return -1;
#endif
    if(ga_conf_readbool("enable-webrtc", 1) == 0) {

      if((m_aencoder = ga_load_module("mod/encoder-audio", "aencoder_")) == NULL)
        return -1;
    }
    //////////////////////////
    }
    if((m_ctrl = ga_load_module("mod/ctrl-sdl", "sdlmsg_replay_")) == NULL)
        return -1;
  if(ga_conf_readbool("enable-webrtc", 1) != 0) {
    if((m_server = ga_load_module("mod/server-webrtc", "live_")) == NULL)
      return -1;
  } else {
    if((m_server = ga_load_module("mod/server-live555", "live555_")) == NULL)
      return -1;
  }
    return 0;
}

int
init_modules() {
    struct RTSPConf *conf = rtspconf_global();
    //static const char *filterpipe[] = { imagepipe0, filterpipe0 };
    if(conf->ctrlenable) {
        ServerConfig serverCfg{};
        serverCfg.prect = (void*)prect;
        ga_init_single_module_or_quit("controller", m_ctrl, &serverCfg, handle_eventreport);
    }
    // controller server is built-in - no need to init
    ga_init_single_module_or_quit("video-encoder", m_vencoder, imagepipefmt, NULL);//filterpipefmt);
    if(ga_conf_readbool("enable-audio", 1) != 0) {
    //////////////////////////
#ifndef __APPLE__
      ga_init_single_module_or_quit("audio-source", m_asource, NULL, NULL);
#endif
      if(ga_conf_readbool("enable-webrtc", 1) == 0) {
        ga_init_single_module_or_quit("audio-encoder", m_aencoder, NULL, NULL);
  }
    //////////////////////////
    }
    //
  if(ga_conf_readbool("enable-webrtc", 1) != 0) {
    ServerConfig webrtcCfg{};
    webrtcCfg.prect = (void *)prect;
    ga_init_single_module_or_quit("server-webrtc", m_server, (void *)(&webrtcCfg), NULL);
  } else if (ga_conf_readbool("enable-quic", 1) != 0) {
    ga_init_single_module_or_quit("server-quic", m_server, NULL, NULL);
  } else {
    ga_init_single_module_or_quit("rtsp-server", m_server, NULL, NULL);
  }
    //
    return 0;
}

int
deinit_modules() {
    struct RTSPConf *conf = rtspconf_global();
    //static const char *filterpipe[] = { imagepipe0, filterpipe0 };
    if (conf->ctrlenable) {
        //ga_module_deinit(m_ctrl, (void *)prect);
    }
    // controller server is built-in - no need to init
    ga_module_deinit(m_vencoder, imagepipefmt);//filterpipefmt);
    if (ga_conf_readbool("enable-audio", 1) != 0) {
        //////////////////////////
#ifndef __APPLE__
        //ga_module_deinit(m_asource, NULL);
#endif
        if (ga_conf_readbool("enable-webrtc", 1) == 0) {
           // ga_module_deinit(m_aencoder, NULL);
        }
        //////////////////////////
    }
    //
    if (ga_conf_readbool("enable-webrtc", 1) != 0) {
        //ga_module_deinit(m_server, (void *)prect);
    }
    else if (ga_conf_readbool("enable-quic", 1) != 0) {
        //ga_module_deinit(m_server, NULL);
    }
    else {
        //ga_module_deinit(m_server, NULL);
    }
    //
    return 0;
}

int
run_modules() {
    struct RTSPConf *conf = rtspconf_global();
    static const char *filterpipe[] =  { imagepipe0, filterpipe0 };
    // controller server is built-in, but replay is a module
    if(conf->ctrlenable) {
        ga_run_single_module_or_quit("control server", ctrl_server_thread, conf);
        // XXX: safe to comment out?
        //ga_run_single_module_or_quit("control replayer", m_ctrl->threadproc, conf);
    }
    // video
    encoder_register_vencoder(m_vencoder, video_encoder_param);
    // audio
    if(ga_conf_readbool("enable-audio", 1) != 0) {
    //////////////////////////
#ifndef __APPLE__
    //ga_run_single_module_or_quit("audio source", m_asource->threadproc, NULL);
    if(m_asource->start(NULL) < 0)        exit(-1);
#endif
  if(ga_conf_readbool("enable-webrtc", 1) == 0) {
    encoder_register_aencoder(m_aencoder, audio_encoder_param);
  }
    //////////////////////////
    }
    // server
    if(m_server->start(NULL) < 0)        exit(-1);
    //
    return 0;
}

int
stop_modules() {
    struct RTSPConf *conf = rtspconf_global();
    static const char *filterpipe[] = { imagepipe0, filterpipe0 };
    // controller server is built-in, but replay is a module
    if (conf->ctrlenable) {
        //ga_run_single_module_or_quit("control server", ctrl_server_thread, conf);
        // XXX: safe to comment out?
        //ga_run_single_module_or_quit("control replayer", m_ctrl->threadproc, conf);
    }
    // video
    m_vencoder->stop(NULL);

    // audio
    if (ga_conf_readbool("enable-audio", 1) != 0) {
        //////////////////////////
#ifndef __APPLE__
    //ga_run_single_module_or_quit("audio source", m_asource->threadproc, NULL);
        if (m_asource->stop(NULL) < 0)        exit(-1);
#endif
        if (ga_conf_readbool("enable-webrtc", 1) == 0) {
            if (m_aencoder->stop(NULL) < 0)    exit(-1);
        }
        //////////////////////////
    }

    // server
    if (m_server->stop(NULL) < 0)        exit(-1);
    //

    return 0;
}

static void *
test_reconfig(void *) {
    int s = 0, err;
    int kbitrate[] = { 3000, 425  };
    int framerate[][2] = { { 12, 1 }, {30, 1}, {24, 1} };
    ga_logger(Severity::INFO, "reconfigure thread started ...\n");
    while(1) {
        ga_ioctl_reconfigure_t reconf;
        if(encoder_running() == 0) {
#ifdef WIN32
            Sleep(1);
#else
            sleep(1);
#endif
            continue;
        }
#ifdef WIN32
        Sleep(3 * 1000);
#else
        sleep(3);
#endif
        bzero(&reconf, sizeof(reconf));
        reconf.id = 0;
        reconf.bitrateKbps = kbitrate[s%2];
#if 0
        reconf.bufsize = 5 * kbitrate[s%2] / 24;
#endif
        // reconf.framerate_n = framerate[s%3][0];
        // reconf.framerate_d = framerate[s%3][1];
        // vsource
        /*
        if(m_vsource->ioctl) {
            err = m_vsource->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
            if(err < 0) {
                ga_logger(Severity::ERR, "reconfigure vsource failed, err = %d.\n", err);
            } else {
                ga_logger(Severity::ERR, "reconfigure vsource OK, framerate=%d/%d.\n",
                        reconf.framerate_n, reconf.framerate_d);
            }
        }
        */
        // encoder
        if(m_vencoder->ioctl) {
            err = m_vencoder->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
            if(err < 0) {
                ga_logger(Severity::ERR, "reconfigure encoder failed, err = %d.\n", err);
            } else {
                ga_logger(Severity::INFO, "reconfigure encoder OK, bitrate=%d; bufsize=%d; framerate=%d/%d.\n",
                        reconf.bitrateKbps, reconf.bufsize,
                        reconf.framerate_n, reconf.framerate_d);
            }
        }
        s = (s + 1) % 6;
    }
    return NULL;
}

void
handle_netreport(ctrlmsg_system_t *msg) {
    ctrlmsg_system_netreport_delay_t *msgn_delay = (ctrlmsg_system_netreport_delay_t *)msg;
    if (msgn_delay->magic == 0xde1a)
    {
        //ga_logger(Severity::ERR, "net-report: first pkt delay %u us, last pkt delay %u us, estimated bandwidth %u kbps\n", msgn_delay->first_delay, msgn_delay->last_delay, msgn_delay->estimated_bw);
    }
    else
    {
        ctrlmsg_system_netreport_t *msgn = (ctrlmsg_system_netreport_t*)msg;
    /*    ga_logger(Severity::ERR, "net-report: capacity=%.3f Kbps; loss-rate=%.2f%% (%u/%u); overhead=%.2f [%u KB received in %.3fs (%.2fKB/s)]\n",
            msgn->capacity / 1024.0,
            100.0 * msgn->pktloss / msgn->pktcount,
            msgn->pktloss, msgn->pktcount,
            1.0 * msgn->pktcount / msgn->framecount,
            msgn->bytecount / 1024,
            msgn->duration / 1000000.0,
            msgn->bytecount / 1024.0 / (msgn->duration / 1000000.0));*/
    }
#if 0
    {
        int err;
        int kbitrate = 0;
        ga_ioctl_reconfigure_t reconf;
        // Reconfigure the bitrates to a new value based on the real network status
        kbitrate =     int (msgn->bytecount*8 / 1024.0 / (msgn->duration / 1000000.0));
        ga_logger(Severity::ERR, "configure the target bitrate to a newer one %d Kbps\n", kbitrate);
        if(encoder_running() != 0) {
           bzero(&reconf, sizeof(reconf));
           reconf.id = 0;
           reconf.bitrateKbps = kbitrate;

           // encoder
           if(m_vencoder->ioctl) {
               err = m_vencoder->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
               if(err < 0) {
                  ga_logger(Severity::ERR, "reconfigure encoder failed, err = %d.\n", err);
               } else {
                  ga_logger(Severity::ERR, "reconfigure encoder OK, bitrate=%d; bufsize=%d; \n",
                        reconf.bitrateKbps, reconf.bufsize);
              }
          }
        }
    }
#endif
}
#include <Windows.h>
#pragma warning(disable: 4005)
#include <ntstatus.h>
#pragma warning(default: 4005)
#include <stdio.h>

typedef long NTSTATUS;
typedef NTSTATUS(NTAPI* pSetTimerResolution)(ULONG RequestedResolution, BOOLEAN Set, PULONG ActualResolution);
typedef NTSTATUS(NTAPI* pQueryTimerResolution)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution);

int setMaximumTimerResolution(void)
{
    NTSTATUS status;
    pSetTimerResolution setFunction;
    pQueryTimerResolution queryFunction;
    ULONG minResolution, maxResolution, actualResolution;
    const HINSTANCE hLibrary = LoadLibrary("NTDLL.dll");
    if (hLibrary == NULL)
    {
        printf("Failed to load NTDLL.dll (%d)\n", GetLastError());
        return 1;
    }

    queryFunction = (pQueryTimerResolution)GetProcAddress(hLibrary, "NtQueryTimerResolution");
    if (queryFunction == NULL)
    {
        printf("NtQueryTimerResolution is null (%d)\n", GetLastError());
        FreeLibrary(hLibrary);
        return 1;
    }

    queryFunction(&minResolution, &maxResolution, &actualResolution);
    printf("Win32 Timer Resolution:\n\tMinimum Value:\t%u\n\tMaximum Value:\t%u\n\tActual Value:\t%u\n\n", minResolution, maxResolution, actualResolution);

    setFunction = (pSetTimerResolution)GetProcAddress(hLibrary, "NtSetTimerResolution");
    if (setFunction == NULL)
    {
        printf("NtSetTimerResolution is null (%d)\n", GetLastError());
        FreeLibrary(hLibrary);
        return 1;
    }

    printf("Setting Timer Resolution to the maximum value (%d)...\n", maxResolution);
    status = setFunction(maxResolution, TRUE, &actualResolution);
    if (status == STATUS_SUCCESS)
    {
        FreeLibrary(hLibrary);
        return 0;
    }

    if (status == STATUS_TIMER_RESOLUTION_NOT_SET)
    {
        printf("Timer not set (Return Code: %d)\n", status);
        FreeLibrary(hLibrary);
        return 2;
    }

    printf("Failed, Return Value: %d (Error Code: %d)\n", status, GetLastError());

    FreeLibrary(hLibrary);
    return 1;
}

// Default values for command line input
static const char* default_loglevel = "info";
// Default file names for backward compatibility
static const char* default_video_stats_file   = "C:\\Temp\\nwstats.csv";
static const char* default_video_bs_file_h264 = "C:\\Temp\\bitstream.h264";
static const char* default_video_bs_file_h265 = "C:\\Temp\\bitstream.h265";
static const char* default_video_bs_file_av1  = "C:\\Temp\\bitstream.av1";
static const char* default_video_raw_file     = "c:\\Temp\\rawcapture.yuv";
static const char* default_enc_frame_number   = "0";

void usage(const char* app)
{
    printf("usage %s [OPTIONS] CONFIG_FILE\n", app);
    printf("options\n");
    printf("  --help                          display this help and exit\n");
    printf("  --logfile <file_name>           Set log file name to <file_name>\n");
    printf("                                  If there is \"PID\" in <file_name>, it will be substituted to Process ID\n");
    printf("  --loglevel <level>              Loglevel to use (default %s)\n", default_loglevel);
    printf("              error               Only errors will be printed\n");
    printf("              warning             Errors and warnings will be printed\n");
    printf("              info                Errors, warnings and info messages will be printed\n");
    printf("              debug               Everything will be printed, including low level debug messages\n");
    printf("  --enable-tcae <0|1>             Enable or disable TCAE\n");
    printf("  --enable-ltr <0|1>              Enable or disable LTR\n");
    printf("  --ltr-interval <number>         Distantce between current frame and referred frame. 0 - QP based; great than 0 - interval based\n");
    printf("  --enable-nwstats <0|1>          Dump encoder stats files %s\n", default_video_stats_file);
    printf("  --video-stats-file <file_name>  Dump encoder stats to the <file_name>\n");
    printf("  --client-stats-file <file_name> Dump client stats to the <file_name>\n");
    printf("  --enable-bs-dump <0|1>          Dump encoder output bitstream by default file name\n");
    printf("                                  Default H.264 bitstream file name is %s\n", default_video_bs_file_h264);
    printf("                                  Default H.265 bitstream file name is %s\n", default_video_bs_file_h265);
    printf("                                  Default AV1 bitstream file name is %s\n", default_video_bs_file_av1);
    printf("  --video-bs-file <file_name>     Dump encoder bitstream to the <file_name>\n");
    printf("  --enable-raw-frame-dump <0|1>   Dump encoder input raw frame to the %s\n", default_video_raw_file);
    printf("  --video-raw-file <file_name>    Enable and dump encoder input raw input to the file\n");
    printf("  --video-codec <h264|avc|h265|hevc|av1>  Use avc|hevc|av1 for encoder\n");
    printf("  --pix_fmt                       Use yuv420p|yuv444p output format for hevc stream\n");
    printf("  --video-bitrate <int>           Video bitrate to use in bits per seconds\n");
    printf("  --enc-trigger-file <file_name>  Encoder start encoding when this file exists\n");
    printf("  --dump-frame-number <number>    Number of frames to dump to debug files (default: 0)\n");
    printf("  --display <name>                Option specifies adapter output by display name.\n");
    printf("                                  Default the first adapter output from the list will be used\n");
    printf("  --server-peer-id                Server peer ID, 0-INT_MAX (default: ga) \n");
    printf("  --client-peer-id                Client peer ID, 0-INT_MAX (default: client) \n");
}

int
main(int argc, char *argv[]) {
    int notRunning = 0;

    std::cout << "Build Version: " << CG_VERSION << std::endl << std::endl;

#ifdef WIN32
    if(CoInitializeEx(NULL, COINIT_MULTITHREADED) < 0) {
        fprintf(stderr, "cannot initialize COM.\n");
        return -1;
    }
    ga_set_process_dpi_aware();
#endif

    int config_idx = 1;
    const char* logfile               = nullptr;
    const char* loglevel              = default_loglevel;
    const char* enable_tcae           = nullptr;
    const char* enable_ltr            = nullptr;
    const char* ltr_interval          = nullptr;
    const char* enable_nwstats        = nullptr;
    const char* video_stats_file      = nullptr;
    const char* client_stats_file     = nullptr;
    const char* enable_bs_dump        = nullptr;
    const char* video_bs_file         = nullptr;
    const char* enable_raw_frame_dump = nullptr;
    const char* video_raw_file        = nullptr;
    const char* video_codec           = nullptr;
    const char* pix_fmt               = nullptr;
    const char* video_bitrate         = nullptr;
    const char* enc_trigger_file      = nullptr;
    const char* dump_frame_number     = default_enc_frame_number;
    const char* display               = nullptr;
    const char* server_peer_id        = nullptr;
    const char* client_peer_id        = nullptr;

    for (config_idx = 1; config_idx < argc; ++config_idx) {
        if (std::string("-h") == argv[config_idx] ||
            std::string("--help") == argv[config_idx]) {
            usage(argv[0]);
            exit(0);
        }
        else if (std::string("--logfile") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            logfile = argv[config_idx];
        }
        else if (std::string("--loglevel") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            loglevel = argv[config_idx];
        }
        else if (std::string("--display") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            display = argv[config_idx];
        }
        else if (std::string("--server-peer-id") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            server_peer_id = argv[config_idx];
        }
        else if (std::string("--client-peer-id") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            client_peer_id = argv[config_idx];
        }
        else if (std::string("--enable-tcae") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enable_tcae = argv[config_idx];
        }
        else if (std::string("--enable-ltr") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enable_ltr = argv[config_idx];
        }
        else if (std::string("--ltr-interval") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            ltr_interval = argv[config_idx];
        }
        else if (std::string("--enable-nwstats") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enable_nwstats = argv[config_idx];
        }
        else if (std::string("--video-stats-file") == argv[config_idx]) { // nwstats
            if (++config_idx >= argc) break;
            video_stats_file = argv[config_idx];
        }
        else if (std::string("--client-stats-file") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            client_stats_file = argv[config_idx];
        }
        else if (std::string("--enable-bs-dump") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enable_bs_dump = argv[config_idx];
        }
        else if (std::string("--video-bs-file") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            video_bs_file = argv[config_idx];
        }
        else if (std::string("--enable-raw-frame-dump") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enable_raw_frame_dump = argv[config_idx];
        }
        else if (std::string("--video-raw-file") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            video_raw_file = argv[config_idx];
        }
        else if (std::string("--video-codec") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            video_codec = argv[config_idx];
        }
        else if (std::string("--pix_fmt") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            pix_fmt = argv[config_idx];
        }
        else if (std::string("--video-bitrate") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            video_bitrate = argv[config_idx];
        }
        else if (std::string("--enc-trigger-file") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            enc_trigger_file = argv[config_idx];
        }
        else if (std::string("--dump-frame-number") == argv[config_idx]) {
            if (++config_idx >= argc) break;
            dump_frame_number = argv[config_idx];
        }
        else {
            if (config_idx == (argc - 1)) {
                break; // next is config file
            }
            else {
                printf("unknown option: %s\n", argv[config_idx]);
                exit(1);
            }
        }
    }

    if(config_idx >= argc) {
        fprintf(stderr, "fatal: invalid option or no config specified\n");
        usage(argv[0]);
        return -1;
    }

    if(ga_init(argv[config_idx]) < 0)
        return -1;

    // setup params from command line
    if (logfile != nullptr)
        ga_conf_writev("logfile", logfile);
    ga_openlog();

    ga_set_loglevel(ga_get_loglevel_enum(loglevel));

    if (display != nullptr)
        ga_conf_writev("display", display);
    if (server_peer_id != nullptr)
        ga_conf_writev("server-peer-id", server_peer_id);
    if (client_peer_id != nullptr)
        ga_conf_writev("client-peer-id", client_peer_id);
    if (enable_tcae != nullptr)
        ga_conf_writev("enable-tcae", enable_tcae);
    if (enable_ltr != nullptr)
        ga_conf_writev("enable-ltr", enable_ltr);
    if (ltr_interval != nullptr)
        ga_conf_writev("ltr-interval", ltr_interval);
    if (enable_nwstats != nullptr)
        ga_conf_writev("enable-nwstats", enable_nwstats);
    if (video_codec != nullptr)
        ga_conf_writev("video-codec", video_codec);
    if (enable_bs_dump != nullptr)
        ga_conf_writev("enable-bs-dump", enable_bs_dump);
    if (enable_raw_frame_dump  != nullptr)
        ga_conf_writev("enable-raw-frame-dump", enable_raw_frame_dump);
    if (ga_is_h265(ga_conf_readstr("video-codec")))
        if (pix_fmt != nullptr)
            ga_conf_writev("pix_fmt", pix_fmt);

    // set default file names for backward compatibility.
    bool nwstats_enabled = ga_conf_readbool("enable-nwstats", false);
    if (nwstats_enabled && (video_stats_file == nullptr))
        video_stats_file = default_video_stats_file;

    bool bs_dump_enabled = ga_conf_readbool("enable-bs-dump", 0);
    if (bs_dump_enabled && (video_bs_file == nullptr)) {
        std::string codec = ga_conf_readstr("video-codec");
        if (!codec.empty()) {
            if (ga_is_h264(codec))
                video_bs_file = default_video_bs_file_h264;
            else if (ga_is_h265(codec))
                video_bs_file = default_video_bs_file_h265;
            else if (ga_is_av1(codec))
                video_bs_file = default_video_bs_file_av1;
            else
                ga_logger(Severity::INFO, "*** unsupported codec.\n");
        }
    }

    bool raw_frame_dump_enabled = ga_conf_readbool("enable-raw-frame-dump", 0);
    if (raw_frame_dump_enabled && (video_raw_file == nullptr))
        video_raw_file = default_video_raw_file;

    if (video_stats_file != nullptr)
        ga_conf_writev("video-stats-file", video_stats_file);
    if (client_stats_file != nullptr)
        ga_conf_writev("client-stats-file", client_stats_file);
    if (video_bs_file != nullptr)
        ga_conf_writev("video-bs-file", video_bs_file);
    if (video_raw_file != nullptr)
        ga_conf_writev("video-raw-file", video_raw_file);
    if (video_bitrate != nullptr)
        ga_conf_mapwritev("video-specific", "b", video_bitrate);
    if (enc_trigger_file != nullptr) {
        ga_conf_writev("enc-trigger-file", enc_trigger_file);
    }
    ga_conf_writev("dump-frame-number", dump_frame_number);

    if(rtspconf_parse(rtspconf_global()) < 0)
    { return -1; }
    //
    prect = NULL;
    //
    if(ga_crop_window(&rect, &prect) < 0) {
        return -1;
    } else if(prect == NULL) {
        ga_logger(Severity::INFO, "*** Crop disabled.\n");
    } else if(prect != NULL) {
        ga_logger(Severity::INFO, "*** Crop enabled: (%d,%d)-(%d,%d)\n",
                  prect->left, prect->top,
                  prect->right, prect->bottom);
    }

    if (prect == NULL) {
        prect = (gaRect *)malloc(sizeof(gaRect));
        if (prect) {
            prect->left = 0;
            prect->top = 0;
            prect->width = GetSystemMetrics(SM_CXSCREEN);
            prect->right = prect->width - 1;
            prect->height = GetSystemMetrics(SM_CYSCREEN);
            prect->bottom = prect->height - 1;
            ga_logger(Severity::INFO, "destination rectangle is empty, setting it to the desktop resolution wxh: %dx%d\n",
                    prect->width, prect->height);
        }
    }

    //
    if(load_modules() < 0 || init_modules() < 0 || run_modules() < 0){
        if (prect) {
            free(prect);
        }
        return -1;
    }

    // enable handler to monitored network status
    ctrlsys_set_handler(CTRL_MSGSYS_SUBTYPE_NETREPORT, handle_netreport);
    //
    //pthread_t t;
    //pthread_create(&t, NULL, test_reconfig, NULL);

    //rtspserver_main(NULL);
    //liveserver_main(NULL);
    setMaximumTimerResolution();

    HANDLE handle = GetCurrentProcess();
    if (handle != NULL) {
        ga_logger(Severity::INFO, " get current process handle success\n");
        BOOL bConfig = SetPriorityClass(handle, REALTIME_PRIORITY_CLASS);
        if (bConfig) {
            ga_logger(Severity::INFO, "configure the process priority success\n");
        }
        else
        {
            ga_logger(Severity::ERR, "Failed to configure the process priority\n");
        }
    }
    else {
        ga_logger(Severity::ERR, " Failed to get the process handle\n");
    }

    while(1) {
#ifdef WIN32
        usleep(1000000);
        if (_kbhit()) {
            char buf = _getch();
            if (buf == 'q') {
                break;
            }
        }
#else
        usleep(5000000);
#endif
    }
    // alternatively, it is able to create a thread to run rtspserver_main:
    //    pthread_create(&t, NULL, rtspserver_main, NULL);

    ga_deinit();
    stop_modules();
    deinit_modules();

    if (prect) {
        free(prect);
    }

    return 0;
}
