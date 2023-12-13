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
  'asource-system.cpp',
  'ga-win32-wasapi.cpp',
  )

srcs += windows.compile_resources(
  files('asource-system.rc'),
  args : winres_args,
  depend_files : [ cgver_file, files('resource.h') ],
  depends : cgvcs_tgt,
  )

cpp_args = ['-DGA_MODULE']

core_deps = [
  cpp.find_library('ole32'),
  ]

shared_library('asource-system', srcs,
  cpp_args: cpp_args,
  dependencies: [core_deps, ga_dep],
  install: true,
  install_dir : modules_install_dir,
  )
