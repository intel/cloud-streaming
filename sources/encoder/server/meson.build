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

srcs = files(
  'cli.cpp',
  'display_server.cpp',
  'display_server_vhal.cpp',
  'display_video_renderer.cpp',
  'main.cpp',
  )

cpp_args = [
  '-DENABLE_QSV',
  '-DBUILD_FOR_HOST=1',
  '-D__STDC_CONSTANT_MACROS',
  ]

executable('icr_encoder', srcs,
  cpp_args : cpp_args,
  dependencies : [
    irrv_dep,
    irr_encoder_dep,
    libavcodec_dep,
    libdrm_dep,
    libva_dep,
    libvhal_dep,
    sock_util_dep,
    thread_dep,
    ],
  install : true,
  )

