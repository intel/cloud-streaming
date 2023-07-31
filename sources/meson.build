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

cgvcs_tgt = vcs_tag(
  command : [ 'git', 'rev-parse', '--short', 'HEAD'],
  input : 'cg-vcs.h.in',
  output : 'cg-vcs.h',
  fallback : 'tarball',
  )

ver = meson.project_version().split('.')

cgver_file = configure_file(
  input : 'cg-version.h.in',
  output : 'cg-version.h',
  configuration : {
    'VER_MAJOR' : ver[0],
    'VER_MINOR' : ver[1],
    'VER_MICRO' : ver[2],
    'VER_REV'   : 0,
    },
)

if host_machine.system() == 'linux'
  subdir('encoder')
endif

if get_option('streamer').enabled()
  subdir('streamer')
endif
