# Copyright (C) 2022 Intel Corporation
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

if("$ENV{ICR_ENABLE_MEMSHARE}" STREQUAL "1")
    Message(status "  " enable_memshare)
    add_definitions(-DENABLE_MEMSHARE)
endif()

add_definitions(-DLOG_TAG=\"irr_encoder\" -DBUILD_FOR_HOST=1 -D__STDC_CONSTANT_MACROS)

set(SRCS
  CCallbackMux.cpp
  CFFDecoder.cpp
  CFFDemux.cpp
  CFFEncoder.cpp
  CFFFilter.cpp
  CFFMux.cpp
  CIrrVideoDemux.cpp
  CQSVAPIDevice.cpp
  CRemoteMux.cpp
  CTransCoder.cpp
  CVAAPIDevice.cpp
  encoder.cpp
  IrrStreamer.cpp
  stream.cpp
  irrv/irrv_protocol.cpp
  utils/CTransLog.cpp
  utils/IORuntimeWriter.cpp
  utils/IOStreamWriter.cpp
  utils/ProfTimer.cpp
  utils/TimeLog.cpp
  tcae/CTcaeWrapper.cpp
  tcae/enc_frame_settings_predictor.cpp
  tcae/net_pred.cpp
  )

add_library( irr_encoder SHARED ${SRCS})

target_include_directories(irr_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(irr_encoder
  PRIVATE
    sock_util
    libavcodec-deps
    libavfilter-deps
    libvpl-deps
    libva-deps
    libvhal-client-deps
    drm-deps
    Threads::Threads
)

target_link_options(irr_encoder PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/irr_encoder.map)

if (IS_ABSOLUTE ${CMAKE_INSTALL_INCLUDEDIR})
  set(PKGCONFIG_HEADER_DIR ${CMAKE_INSTALL_INCLUDEDIR})
else()
  set(PKGCONFIG_HEADER_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
endif()

if (IS_ABSOLUTE ${CMAKE_INSTALL_LIBDIR})
  set(PKGCONFIG_LIB_DIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
else()
  set(PKGCONFIG_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/irrv/irrv.pc.in ${CMAKE_CURRENT_BINARY_DIR}/irrv.pc @ONLY)

install(TARGETS irr_encoder LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_LIST_DIR}/irrv/irrv_protocol.h DESTINATION ${PKGCONFIG_HEADER_DIR}/irrv/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/irrv.pc DESTINATION ${PKGCONFIG_LIB_DIR})

