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

if host_machine.system() == 'windows'
  modules_install_dir = get_option('bindir') / 'mod'
else
  modules_install_dir = get_option('libdir') / 'ga'
endif

if host_machine.system() == 'windows'
  subdir('asource-system')
  subdir('ctrl-sdl')
  if build_machine.cpu_family() != 'x86'
    subdir('desktop-capture')
  endif
endif

if host_machine.system() == 'linux'
  subdir('irrv-receiver')
endif

subdir('server-webrtc')
