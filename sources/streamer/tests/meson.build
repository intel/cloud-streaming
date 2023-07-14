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

irrv_srcs = files(
  '../module/irrv-receiver/CSendRecvMessage.cpp',
  '../module/irrv-receiver/sock_client.cpp',
  )

gtest_dep = dependency('gtest')
gtest_main_dep = dependency('gtest_main')

executable('irrv-client-test', [files('irrv_client_test.cpp'), irrv_srcs],
  cpp_args : ['-DHOST_BUILD'],
  include_directories : include_directories('../module/irrv-receiver'),
  dependencies: [ga_dep, gtest_dep, irrv_dep, thread_dep],
  install : true,
  )

executable('irrv-client', [files('irrv_client.cpp'), irrv_srcs],
  cpp_args : ['-DHOST_BUILD'],
  include_directories : include_directories('../module/irrv-receiver'),
  dependencies: [ga_dep, irrv_dep, thread_dep],
  install : true,
  )

