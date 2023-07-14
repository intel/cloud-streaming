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

usage = 'android-cg/'

install_data(usage + 'server.desktop.webrtc.conf',
  install_dir : get_option('sysconfdir') / 'ga')
install_data(
  usage + 'common/audio-pcm.conf',
  usage + 'common/controller.conf',
  usage + 'common/server-common.conf',
  usage + 'common/video-x264.conf',
  usage + 'common/video-x264-param.conf',
  usage + 'common/camera.conf',
  install_dir : get_option('sysconfdir') / 'ga' / 'common')

