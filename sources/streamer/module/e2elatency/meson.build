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
  'E2ELatencyClient.cpp',
  'E2ELatencyFactory.cpp',
  'E2ELatencyServer.cpp',
  )

e2e_proto = custom_target('e2e_proto',
  input : 'gae2elatency.proto',
  output : ['gae2elatency.pb.h', 'gae2elatency.pb.cc'],
  command : [
    protoc,
    '--cpp_out=@OUTDIR@',
    '-I=@CURRENT_SOURCE_DIR@',
    # We can't use @INPUT@ which would be more correct from meson perspective
    # for the reason that protoc has a requirement that -I or --proto_path
    # must be exact prefix of .proto file names otherwise protoc aborts with
    # an error. Unfortunately meson did not guarantee that for the @CURRENT_SOURCE_DIR@
    # and @INPUT@ pair.
    '@CURRENT_SOURCE_DIR@/gae2elatency.proto'
    ],
  )

_e2e_static_lib = static_library('e2e-latency', [srcs, e2e_proto],
  dependencies: protobuf_dep,
  install: false,
  )

e2e_static_dep = declare_dependency(
  include_directories : include_directories('.'),
  link_with : _e2e_static_lib,
)

