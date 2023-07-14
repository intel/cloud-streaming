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
  'CSendRecvMessage.cpp',
  'irrv-receiver.cpp',
  'sock_client.cpp',
  )

cpp_args = ['-DGA_MODULE', '-DHOST_BUILD']

shared_library('irrv-receiver', srcs,
  cpp_args : cpp_args,
  dependencies : [ga_dep, irrv_dep, thread_dep],
  name_prefix : '',
  install: true,
  install_dir : modules_install_dir,
  )

