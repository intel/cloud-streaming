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
  'ga-cursor.cpp',
  'ga-qos.cpp',
  'ga-video-input.cpp',
  'ics-p2p-client.cpp',
  'p2p-socket-signaling-channel.cpp',
  'server-webrtc.cpp',
  )

if host_machine.system() == 'windows'
  srcs += files(
    'ga-audio-input.cpp',
    'ga-controller-sdl.cpp',
    )
endif

if host_machine.system() == 'linux'
  srcs += files(
    'ga-controller-android.cpp',
    'aic-vhal-client/audio-frame-generator.cpp',
    'aic-vhal-client/AudioPlayer.cpp',
    'aic-vhal-client/EncodedVideoDispatcher.cpp',
    'aic-vhal-client/RemoteStreamHandler.cpp',
    'aic-vhal-client/SensorHandler.cpp',
    'aic-vhal-client/CommandChannelHandler.cpp',
    'aic-vhal-client/CameraClientHandler.cpp',
    'aic-vhal-client/android-latency.cpp',
  )
endif

deps = [
  ga_dep,
  nlohmann_json_dep,
  owt_dep,
  sioclient_dep,
  thread_dep,
  ]

cpp_args = ['-DGA_MODULE', '-DOWT_CG_SERVER']
inc_dirs = []
if host_machine.system() == 'windows'
  cpp_args += ['-DWEBRTC_WIN']
  deps += cpp.find_library('iphlpapi')
else
  cpp_args += ['-DWEBRTC_LINUX', '-DWEBRTC_POSIX']
  inc_dirs += include_directories('aic-vhal-client')
  deps += libvhal_dep
endif

shared_library('server-webrtc', srcs,
  cpp_args : cpp_args,
  include_directories : inc_dirs,
  dependencies : deps,
  name_prefix : '',
  install: true,
  install_dir : modules_install_dir,
  )

