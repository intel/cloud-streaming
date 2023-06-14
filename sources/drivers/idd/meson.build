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

# 'idd_install_dir' is location where built IDD driver should be installed.
# Mind that currently we don't build IDD driver by meson. Instead, user should
# build it using provided 'IddSampleDriver.sln' MSVC solution file. Here in
# meson script we are just doing simple book keeping for the installation path
# to limit work done by MSVC solution to the bare minimum. In a future once
# meson support to build Windows UMDF/KMDF will mature we will switch to building
# driver by meson.
# See: https://github.com/mesonbuild/meson/issues/11965
idd_install_dir = get_option('bindir') / 'idd'

# 'idd_src_install_dir' is location to install IDD sources
idd_src_install_dir = get_option('prefix') / 'src' / 'drivers' / 'idd'

install_data(
  files(
    'LICENSE',
    ),
  install_dir : idd_install_dir)
