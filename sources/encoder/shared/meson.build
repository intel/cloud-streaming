# Copyright (C) 2023 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

libavcodec_dep = dependency('libavcodec', required : true)
libavfilter_dep = dependency('libavfilter', required : true)
libavformat_dep = dependency('libavformat', required : true)
libavutil_dep = dependency('libavutil', required : true)
libdrm_dep = dependency('libdrm', required : true)
libva_dep = dependency('libva', required : true)
libvhal_dep = dependency('vhal-client', required : true)
libvpl_dep = dependency('vpl', required : true)

srcs = files(
  'CCallbackMux.cpp',
  'CFFDecoder.cpp',
  'CFFDemux.cpp',
  'CFFEncoder.cpp',
  'CFFFilter.cpp',
  'CFFMux.cpp',
  'CIrrVideoDemux.cpp',
  'CQSVAPIDevice.cpp',
  'CRemoteMux.cpp',
  'CTransCoder.cpp',
  'CVAAPIDevice.cpp',
  'encoder.cpp',
  'IrrStreamer.cpp',
  'stream.cpp',
  'irrv/irrv_protocol.cpp',
  'utils/CTransLog.cpp',
  'utils/IORuntimeWriter.cpp',
  'utils/IOStreamWriter.cpp',
  'utils/ProfTimer.cpp',
  'utils/TimeLog.cpp',
  'tcae/CTcaeWrapper.cpp',
  'tcae/enc_frame_settings_predictor.cpp',
  'tcae/net_pred.cpp',
  )

cpp_args = [
  '-DENABLE_QSV',
  '-DLOG_TAG="irr_encoder"',
  '-DBUILD_FOR_HOST=1',
  '-D__STDC_CONSTANT_MACROS',
  ]
link_args = ['-Wl,--version-script', meson.current_source_dir() / 'irr_encoder.map']

_lib = library('irr_encoder', srcs,
  cpp_args : cpp_args,
  link_args : link_args,
  link_depends : files('irr_encoder.map'),
  dependencies : [
    irrv_dep,
    libavcodec_dep,
    libavfilter_dep,
    libavformat_dep,
    libavutil_dep,
    libdrm_dep,
    libva_dep,
    libvhal_dep,
    libvpl_dep,
    sock_util_dep,
    thread_dep,
    ],
  install : true,
  )

irr_encoder_dep = declare_dependency(
  include_directories : include_directories('.'),
  link_with : _lib
  )

