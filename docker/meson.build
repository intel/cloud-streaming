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

m4_args = [
  '-E', # treat warnings as errors
  '-D', 'DEVEL',
  '-I', meson.source_root() / 'templates',
  '-I', meson.source_root() / 'templates' / 'm4docker' / 'system',
  '-I', meson.source_root() / 'templates' / 'm4docker' / 'components',
  ]

m4_templates = files(
  '../templates/m4docker/system/begin.m4',
  '../templates/m4docker/system/centos-repo.m4',
  '../templates/m4docker/system/end.m4',
  '../templates/m4docker/system/ubuntu.m4',
  '../templates/m4docker/components/cmake.m4',
  '../templates/m4docker/components/gmmlib.m4',
  '../templates/m4docker/components/intel-gfx.m4',
  '../templates/m4docker/components/libva2.m4',
  '../templates/m4docker/components/libva2-utils.m4',
  '../templates/m4docker/components/media-driver.m4',
  '../templates/m4docker/components/msdk.m4',
  '../templates/m4docker/components/nodesource.m4',
  '../templates/m4docker/components/onevpl-gpu.m4',
  '../templates/m4docker/components/onevpl.m4',
  '../templates/chrome.m4',
  '../templates/defs.m4',
  '../templates/encoder.m4',
  '../templates/ffmpeg.m4',
  '../templates/git.m4',
  '../templates/libvhal-client.m4',
  '../templates/owt.m4',
  '../templates/owt-p2p-server.m4',
  '../templates/owt.prebuilt.m4',
  '../templates/sioclient.m4',
  '../templates/streamer.m4',
  )

subdir('encoder')
subdir('streamer')

subdir('jstest')
subdir('owt')
subdir('p2p')

