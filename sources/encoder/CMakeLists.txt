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

cmake_minimum_required( VERSION 3.13 )
project(display-daemon)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "CMAKE_BUILD_TYPE is undefined. Set default build type ${CMAKE_BUILD_TYPE}.")
endif()

include(GNUInstallDirs)

find_library(M_LIB m)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-Wall ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS -Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack ${CMAKE_EXE_LINKER_FLAGS})
set(CMAKE_SHARED_LINKER_FLAGS -Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack ${CMAKE_SHARED_LINKER_FLAGS})

set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(PKG_LIBDRM REQUIRED libdrm>=2.4.98 IMPORTED_TARGET)
pkg_check_modules(PKG_LIBDRM_INTEL REQUIRED libdrm_intel>=2.4.98 IMPORTED_TARGET)

add_library(drm-deps INTERFACE)
target_link_libraries(drm-deps INTERFACE PkgConfig::PKG_LIBDRM_INTEL PkgConfig::PKG_LIBDRM)

pkg_check_modules(VHAL REQUIRED vhal-client IMPORTED_TARGET)

add_library(libvhal-client-deps INTERFACE)
target_link_libraries(libvhal-client-deps INTERFACE PkgConfig::VHAL)

pkg_check_modules(VPL REQUIRED vpl IMPORTED_TARGET)

add_library(libvpl-deps INTERFACE)
# See: https://gitlab.kitware.com/cmake/cmake/-/issues/17377
# cmake does not populate PkgConfig::VPL correctly since it enlists
# libs outside of -Lpath such as -ldl
target_include_directories(libvpl-deps INTERFACE ${VPL_INCLUDE_DIRS})
target_link_libraries(libvpl-deps INTERFACE ${VPL_LDFLAGS})

add_definitions("-DENABLE_QSV")

pkg_check_modules(AVCODEC REQUIRED libavcodec libavformat libavutil IMPORTED_TARGET)
pkg_check_modules(AVFILTER REQUIRED libavfilter IMPORTED_TARGET)

add_library(libavcodec-deps INTERFACE)
target_link_libraries(libavcodec-deps INTERFACE PkgConfig::AVCODEC)

add_library(libavfilter-deps INTERFACE)
target_link_libraries(libavfilter-deps INTERFACE PkgConfig::AVFILTER)

pkg_check_modules(VA REQUIRED libva IMPORTED_TARGET)

add_library(libva-deps INTERFACE)
target_link_libraries(libva-deps INTERFACE PkgConfig::VA)

add_definitions(-DBUILD_FOR_HOST=1)

add_definitions(-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-misleading-indentation -Wno-deprecated-declarations)

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(server)
add_subdirectory(shared)
add_subdirectory(sock_util)

