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

install_headers('irrv_protocol.h',  subdir : 'irrv')

irrv_dep = declare_dependency(
  include_directories : include_directories('..'),
  )

pkg.generate(
  name : 'irrv',
  description : 'Intel(R) IRRV communication protocol',
  version : meson.project_version(),
  )

