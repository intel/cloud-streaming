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

if (get_option('streamer').allowed() or
    get_option('capture').require(host_machine.system() == 'windows').allowed())
  subdir('core')
  if host_machine.system() == 'windows' and build_machine.cpu_family() != 'x86'
    subdir('capture')
  endif
endif

if get_option('streamer').allowed()
  streamer_required = get_option('streamer').enabled() or host_machine.system() == 'linux'

  build_streamer = dependency('owt', required : streamer_required).found()

  # These are ingredients which we don't need to configure if we can't
  # build any of them. These are not entrypoints, so we don't need to
  # report them in a summary as well.
  if (build_streamer)
    subdir('config')
    subdir('module')
  endif

  subdir('server')

  if host_machine.system() == 'linux'
    subdir('tests')
  endif
endif
