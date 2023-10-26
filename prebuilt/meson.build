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

# Using declare_dependency() wrapped by meson.override_dependency() to make
# sure that dependency is fetched explicitly with dependency() rather than
# used via variable. So, the style we use is:
#
#   meson.override_dependency('...', declare_dependency(...))
#

if host_machine.system() == 'linux'
  meson.override_dependency('owt', declare_dependency(
    include_directories : include_directories('owt/opt/include'),
    dependencies : [
      cc.find_library('owt', dirs : meson.current_source_dir() / 'owt/opt/lib'),
      ],
    ))
endif
