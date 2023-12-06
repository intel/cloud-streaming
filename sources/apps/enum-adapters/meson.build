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

summary({'enum-adapters' : true}, bool_yn : true, section: 'Targets')

srcs = [cgver_file, cgvcs_tgt]
srcs += files(
  'enum-adapters.cpp',
  'query-adapters.cpp',
  )

srcs += windows.compile_resources(
  files('enum-adapters.rc'),
  args : winres_args,
  depend_files : [ cgver_file, files('resource.h') ],
  depends : cgvcs_tgt,
  )

core_deps = [
  cpp.find_library('dxgi'),
  cpp.find_library('gdi32'),
  cpp.find_library('ole32'),
  cpp.find_library('user32'),
  ]

executable('enum-adapters', srcs,
  dependencies : core_deps,
  install : true,
  )
