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

srcs = [cgver_file, cgvcs_tgt]
srcs += files(
  'ga-common.cpp',
  'ga-conf.cpp',
  'ga-confvar.cpp',
  'ga-module.cpp',
  'dpipe.cpp',
  'vsource.cpp',
  'encoder-common.cpp',
  )

cpp_public_args = []
cpp_args = []
core_deps = []

if host_machine.system() == 'windows'
  srcs += files(
    'asource.cpp',
    'ga-get-opts.cpp',
    'ga-param-shared.cpp',
    'controller.cpp',
    'ctrl-msg.cpp',
    'cursor.cpp',
    'ga-win32.cpp',
    'rtspconf.cpp',
    'QoSMgt.cpp',
    )

  srcs += windows.compile_resources(
    files('libga.rc'),
    args : winres_args,
    depend_files : [ cgver_file, files('resource.h') ],
    depends : cgvcs_tgt,
    )

  cpp_public_args += [
    '-DHAVE_STRUCT_TIMESPEC',
    '-DWIN32',
    '-D_MBCS',
    '-DUSE_GA_WIN32_MACRO',
    '-D__STDC_CONSTANT_MACROS',
    '-D__STDC_LIMIT_MACROS',
    ]

  cpp_args += cpp_public_args + '-DGA_LIB'
  core_deps += [
    cpp.find_library('dxgi'),
    cpp.find_library('ws2_32'),
    ]
endif

_libga = shared_library('ga', srcs,
  cpp_args: cpp_args,
  include_directories: include_directories('.'),
  dependencies: [core_deps, dl_dep, thread_dep],
  install: true,
  )

ga_dep = declare_dependency(
  compile_args: cpp_public_args,
  include_directories: include_directories('.'),
  link_with: _libga,
  sources: [cgver_file, cgvcs_tgt],
)

