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

project('cloud-gaming', 'c', 'cpp',
  version : '0.5.0',
  default_options : [
    'cpp_std=c++17',
    'b_ndebug=if-release',
    'b_pie=true',
    'b_vscrt=mt',
    'c_winlibs=',
    'cpp_winlibs=',
    ],
  )

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

_trial_c = ['-Wformat', '-Wformat-security']

c_args = cc.get_supported_arguments(_trial_c)
cpp_args = cpp.get_supported_arguments(_trial_c)

add_project_arguments(c_args,   language : ['c'])
add_project_arguments(cpp_args, language : ['cpp'])

_trial_link = ['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack']

c_link_args = cc.get_supported_link_arguments(_trial_link)
cpp_link_args = cpp.get_supported_link_arguments(_trial_link)

add_project_link_arguments(c_link_args, language : ['c'])
add_project_link_arguments(c_link_args, language : ['cpp'])

fs = import('fs')
pkg = import('pkgconfig')
windows = import('windows')

m4 = find_program('m4', required : get_option('m4'))
if m4.found()
  subdir('docker')
endif

null_dep = dependency('', required : false)

dl_dep = null_dep
if host_machine.system() != 'windows'
  if not cc.has_function('dlopen')
    dl_dep = cc.find_library('dl', required : true)
  endif
endif
thread_dep = dependency('threads')

if host_machine.system() == 'linux'
  libdrm_dep = dependency('libdrm', required : get_option('encoder'))
  libvhal_dep = dependency('vhal-client',
    required : get_option('encoder').enabled() or get_option('streamer').enabled())
  libvpl_dep = dependency('vpl', required : get_option('encoder'))
  libva_dep = dependency('libva', required : get_option('encoder'))

  libavcodec_dep = dependency('libavcodec', required : get_option('encoder'))
  libavfilter_dep = dependency('libavfilter', required : get_option('encoder'))
  libavformat_dep = dependency('libavformat', required : get_option('encoder'))
  libavutil_dep = dependency('libavutil', required : get_option('encoder'))

  titansdk_dep = null_dep
endif

if host_machine.system() == 'windows'
  winres_args = []
  if build_machine.cpu_family() == 'x86_64'
    winres_args += '-D_WIN64'
  endif
endif

nlohmann_json_dep = dependency('nlohmann_json', required : get_option('streamer'))

sioclient_dep = dependency('sioclient', required : get_option('streamer'))

subdir('prebuilt')
subdir('sources')
