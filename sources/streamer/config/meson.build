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

if host_machine.system() == 'linux'
  install_data(
    files(
      'android-cg/server.desktop.webrtc.conf',
      ),
    install_dir : get_option('sysconfdir') / 'ga')
  install_data(
    files(
      'android-cg/common/audio-pcm.conf',
      'android-cg/common/controller.conf',
      'android-cg/common/server-common.conf',
      'android-cg/common/video-x264.conf',
      'android-cg/common/video-x264-param.conf',
      'android-cg/common/camera.conf',
      ),
    install_dir : get_option('sysconfdir') / 'ga' / 'common')
endif

if host_machine.system() == 'windows' and build_machine.cpu_family() != 'x86'
  install_data(
    files(
      'server.desktop.webRTC.conf',
      'server.desktop.webRTC.hevc.conf',
      'server.desktop.webRTC.av1.conf',
      ),
    install_dir : get_option('prefix') / 'config')
  install_data(
    files(
      'common/audio-lame.conf',
      'common/audio-opus.conf',
      'common/audio-pcm.conf',
      'common/controller.conf',
      'common/server-common.conf',
      'common/video-x264.conf',
      'common/video-x264-param.conf',
      ),
    install_dir : get_option('prefix') / 'config' / 'common')
endif
