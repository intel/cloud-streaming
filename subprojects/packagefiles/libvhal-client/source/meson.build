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
  'audio_sink.cc',
  'audio_source.cc',
  'command_channel_interface.cc',
  'hwc_profile_log.cc',
  'hwc_vhal.cc',
  'sensor_interface.cc',
  'tcp_stream_socket_client.cc',
  'unix_stream_socket_client.cc',
  'video_sink.cc',
  'virtual_gps_receiver.cc',
  'virtual_input_receiver.cc',
  'vsock_stream_socket_client.cc',
  )

inc_dirs = include_directories(
  '../include',
  '../include/libvhal',
  )

_lib = library('vhal-client', srcs,
  include_directories : inc_dirs,
  dependencies : [thread_dep],
  )

libvhal_dep = declare_dependency(
  include_directories : inc_dirs,
  link_with : _lib,
  )
meson.override_dependency('vhal-client', libvhal_dep)

