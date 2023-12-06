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

capture_required = get_option('capture').enabled() or get_option('streamer').enabled()

libavcodec_dep = dependency('libavcodec', required : capture_required)
libavutil_dep = dependency('libavutil', required : capture_required)
libvpl_dep = dependency('vpl', required : capture_required)

deps = [
  libavcodec_dep,
  libavutil_dep,
  libvpl_dep,
  ]

foreach dep : deps
  if not dep.found()
    warning('capture will NOT be built due to missing dependencies')
    capture_dep = null_dep
    subdir_done()
  endif
endforeach

srcs = files(
  'av-qsv-encoder.cpp',
  'av-qsv-encoder.h',
  'av-utils.cpp',
  'av-utils.h',
  'cursor-provider.h',
  'cursor-receiver.cpp',
  'cursor-receiver.h',
  'desktop-duplicator.cpp',
  'desktop-duplicator.h',
  'dt-capture.cpp',
  'dt-capture.h',
  'dx-utils.cpp',
  'dx-utils.h',
  'dx11-surface-pool.cpp',
  'dx11-surface-pool.h',
  'dx11-surface.cpp',
  'dx11-surface.h',
  'dx12-surface-pool.cpp',
  'dx12-surface-pool.h',
  'dx12-surface.cpp',
  'dx12-surface.h',
  'encoder.h',
  'encoder.cpp',
  'event-queue.h',
  'event-queue.cpp',
  'frame-provider.h',
  'frame.cpp',
  'frame.h',
  'surface-pool.h',
  'surface.h',
  'video-processor.cpp',
  'video-processor.h',
  )

cpp_args = ['-DGA_SERVER', '-DGA_MODULE']

core_deps = [
  cpp.find_library('d3d11'),
  cpp.find_library('d3d12'),
  cpp.find_library('dxgi')
  ]

_lib = static_library('capture', srcs,
  cpp_args : cpp_args,
  dependencies : [core_deps, ga_dep, deps],
  install : false,
  )

capture_dep = declare_dependency(
  include_directories : include_directories('.'),
  link_with : _lib
  )
