# Copyright (C) 2022-2023 Intel Corporation
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

add_executable(icr_encoder
    cli.cpp
    main.cpp
    display_server.cpp
    display_server_vhal.cpp
    display_video_renderer.cpp)

target_link_libraries(icr_encoder
    irrv-protocol
    irr_encoder
    sock_util
    ${CMAKE_DL_LIBS}
    ${M_LIB}
    libvhal-client-deps
    libavcodec-deps
    libva-deps
    drm-deps)

install(TARGETS icr_encoder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

