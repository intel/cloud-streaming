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

project('sioclient', 'cpp')

asio_dep = dependency('asio')
rapidjson_dep = dependency('rapidjson')
websocketpp_dep = dependency('websocketpp')
openssl_dep = dependency('openssl')

srcs = files(
  'src/sio_client.cpp',
  'src/sio_socket.cpp',
  'src/internal/sio_client_impl.cpp',
  'src/internal/sio_packet.cpp',
  )

cpp_args = [
  '-DASIO_STANDALONE',
  '-D_WEBSOCKETPP_CPP11_STL_',
  '-D_WEBSOCKETPP_CPP11_FUNCTIONAL_',
  ]

deps = [asio_dep, rapidjson_dep, websocketpp_dep]

_lib = static_library('sioclient', srcs,
  cpp_args : cpp_args,
  dependencies : deps,
  )

sioclient_dep = declare_dependency(
  include_directories : include_directories('src'),
  link_with : _lib,
  )
meson.override_dependency('sioclient', sioclient_dep)

if openssl_dep.found()
  _tls_lib = static_library('sioclient_tls', srcs,
    cpp_args : [cpp_args, '-DSIO_TLS'],
    dependencies : [deps, openssl_dep],
    )

  sioclient_tls_dep = declare_dependency(
    include_directories : include_directories('src'),
    link_with : _tls_lib,
    )
  meson.override_dependency('sioclient_tls', sioclient_tls_dep)
endif

