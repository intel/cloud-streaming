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

if not build_streamer
  warning('ga-server-periodic will NOT be built due to missing dependencies')
  summary({'ga-server-periodic' : false}, bool_yn : true, section: 'Targets')
  subdir_done()
endif

summary({'ga-server-periodic' : true}, bool_yn : true, section: 'Targets')

srcs = files('ga-server-periodic.cpp')

if fs.is_absolute(get_option('libdir'))
  cpp_args = ['-DGA_MODULE_PREFIX="' + get_option('libdir') + '/ga/"']
else
  cpp_args = ['-DGA_MODULE_PREFIX="' + get_option('prefix') + '/' + get_option('libdir') + '/ga/"']
endif

executable('ga-server-periodic', srcs,
  cpp_args: cpp_args,
  dependencies: [ga_dep, thread_dep],
  install : true,
  )

