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

imgui_dep = dependency('imgui', required : get_option('client'),
  default_options: ['default_library=static'])
implot_dep = dependency('implot', required : get_option('client'),
  default_options: ['default_library=static'])

libavcodec_dep = dependency('libavcodec', required : get_option('client'))
libavutil_dep = dependency('libavutil', required : get_option('client'))

openssl_dep = dependency('openssl', required : get_option('client'))
owt_client_dep = dependency('owt-client', required : get_option('client'))
rapidjson_dep = dependency('rapidjson', required : get_option('client'))
sioclient_tls_dep = dependency('sioclient_tls', required : openssl_dep.found())

deps = [
  imgui_dep,
  implot_dep,
  libavcodec_dep,
  libavutil_dep,
  owt_client_dep,
  rapidjson_dep,
  sioclient_tls_dep,
  ]

# Exit if 'client' was set to 'auto' and any of dependencies were not found. If
# 'client' was set to 'enabled' we will stop earlier with error processing
# dependencies search above (due to 'required: true').
foreach dep : deps
  if not dep.found()
    warning('webrtc-client will NOT be built due to missing dependencies')
    summary({'webrtc-client' : false}, bool_yn : true, section: 'Targets')
    subdir_done()
  endif
endforeach

summary({'webrtc-client' : true}, bool_yn : true, section: 'Targets')

srcs = [
  # header files
  'control-handler.h',
  'ga-option.h',
  'game-session.h',
  'peer-connection.h',
  'rtc-signaling.h',
  'statistics-window-class.h',
  'video-renderer.h',
  'window-class.h',
  'window-handler.h',
  # source files
  'control-handler.cc',
  'ga-option.cc',
  'game-session.cc',
  'implot-window.cc',
  'peer-connection.cc',
  'rtc-signaling.cc',
  'statistics-window-class.cc',
  'video-renderer.cc',
  'webrtc-client.cc',
  'window-class.cc',
  'window-handler.cc',
  ]

cpp_args = ['-DWEBRTC_WIN', '-DNOMINMAX', '-DUNICODE', '-D_UNICODE']

core_deps = [
  cpp.find_library('d3d11'),
  cpp.find_library('dxgi'),
  cpp.find_library('iphlpapi'),
  cpp.find_library('winmm'),
  ]

executable('webrtc-client', srcs,
  cpp_args : cpp_args,
  win_subsystem : 'windows',
  dependencies : [ deps, core_deps],
  install : true,
  )
