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

# We do require very specific layout for 'prebuilt-path':
#
# * For Linux:
#     get_option('prebuilt-path') / opt / bin     : executables
#     get_option('prebuilt-path') / opt / include : include files
#     get_option('prebuilt-path') / opt / lib     : dynamic and static libraries
#   NOTE: basically 'prebuilt-path' is DESTDIR. You can install it with:
#     COPY /opt/dist /
#
# * For Windows:
#     get_option('prebuilt-path') / bin           : executables
#     get_option('prebuilt-path') / include       : include files
#     get_option('prebuilt-path') / lib           : dynamic and static libraries

prebuilt_path = get_option('prebuilt-path')

# find_library() requires absolute paths. include_directories() requires
# relative paths for project source directories and accepts absolute paths (as
# it seems) for external directories.
if prebuilt_path != ''
  if fs.is_absolute(prebuilt_path)
    prebuilt_path_abs = prebuilt_path
  else
    prebuilt_path_abs = meson.source_root() / prebuilt_path
    # recalculate relative path from the current directory
    prebuilt_path = '..' / prebuilt_path
  endif
else
  prebuilt_path = host_machine.system() + '-' + build_machine.cpu_family()
  prebuilt_path_abs = meson.current_source_dir() / prebuilt_path
endif

if not fs.exists(prebuilt_path)
  warning('Invalid prebuilt-path provided.',
    'Project might fail to build due to missing dependencies.')
  subdir_done()
endif

# Using declare_dependency() wrapped by meson.override_dependency() to make
# sure that dependency is fetched explicitly with dependency() rather than
# used via variable. So, the style we use is:
#
#   meson.override_dependency('...', declare_dependency(...))
#

if host_machine.system() == 'linux'
  meson.override_dependency('owt', declare_dependency(
    include_directories : include_directories(prebuilt_path / 'opt' / 'include'),
    dependencies : [
      cpp.find_library('owt', dirs : prebuilt_path_abs / 'opt' / 'lib'),
      ],
    ))
endif

if host_machine.system() == 'windows'
  if cpp.find_library('owt', dirs : prebuilt_path_abs / 'lib', required: false).found()
    meson.override_dependency('owt', declare_dependency(
      include_directories : include_directories(prebuilt_path / 'include'),
      compile_args : ['-DWEBRTC_WIN'],
      dependencies : cpp.find_library('owt', dirs : prebuilt_path_abs / 'lib'),
      ))
  endif

  if build_machine.cpu_family() == 'x86_64'
    if cpp.find_library('owt-client', dirs : prebuilt_path_abs / 'lib', required: false).found()
      meson.override_dependency('owt-client', declare_dependency(
        include_directories : include_directories(prebuilt_path / 'include'),
        compile_args : ['-DWEBRTC_WIN'],
        dependencies : cpp.find_library('owt-client', dirs : prebuilt_path_abs / 'lib'),
        ))
    endif

    # NOTE : cc.find_library('xxx.dll') searches for the static library named libxxx.dll.a

    install_files = []

    if (cpp.find_library('crypto.dll', dirs : prebuilt_path_abs / 'lib', required: false).found() and
        cpp.find_library('ssl.dll', dirs : prebuilt_path_abs / 'lib', required: false).found())
      meson.override_dependency('libcrypto', declare_dependency(
        include_directories : include_directories(prebuilt_path / 'include'),
        dependencies : cc.find_library('crypto.dll', dirs : prebuilt_path_abs / 'lib'),
        ))
      meson.override_dependency('libssl', declare_dependency(
        include_directories : include_directories(prebuilt_path / 'include'),
        dependencies : cc.find_library('ssl.dll', dirs : prebuilt_path_abs / 'lib'),
        ))
      meson.override_dependency('openssl', declare_dependency(
        dependencies: [dependency('libcrypto'), dependency('libssl')],
        ))
      install_files += files(
        prebuilt_path_abs / 'bin' / 'libcrypto-3-x64.dll',
        prebuilt_path_abs / 'bin' / 'libssl-3-x64.dll',
        )
    endif

    if (cpp.find_library('avcodec.dll', dirs : prebuilt_path_abs / 'lib', required: false).found() and
        cpp.find_library('avutil.dll', dirs : prebuilt_path_abs / 'lib', required: false).found())
      meson.override_dependency('libavcodec', declare_dependency(
        dependencies : cc.find_library('avcodec.dll', dirs : prebuilt_path_abs / 'lib'),
        include_directories : include_directories(prebuilt_path / 'include'),
        ))
      meson.override_dependency('libavutil', declare_dependency(
        dependencies : cc.find_library('avutil.dll', dirs : prebuilt_path_abs / 'lib'),
        include_directories : include_directories(prebuilt_path / 'include'),
        ))
      install_files += files(
        prebuilt_path_abs / 'bin' / 'avcodec-60.dll',
        prebuilt_path_abs / 'bin' / 'avdevice-60.dll',
        prebuilt_path_abs / 'bin' / 'avfilter-9.dll',
        prebuilt_path_abs / 'bin' / 'avformat-60.dll',
        prebuilt_path_abs / 'bin' / 'swresample-4.dll',
        prebuilt_path_abs / 'bin' / 'swscale-7.dll',
        prebuilt_path_abs / 'bin' / 'avutil-58.dll',
        )
    endif

    if cpp.find_library('vpl.dll', dirs : prebuilt_path_abs / 'lib', required: false).found()
      meson.override_dependency('vpl', declare_dependency(
        dependencies : cc.find_library('vpl.dll', dirs : prebuilt_path_abs / 'lib'),
        include_directories : include_directories(
          prebuilt_path / 'include',
          prebuilt_path / 'include' / 'vpl',
          ),
        ))
      install_files += files(
        prebuilt_path_abs / 'bin' / 'libvpl.dll',
        # C++ deps
        prebuilt_path_abs / 'bin' / 'libgcc_s_seh-1.dll',
        prebuilt_path_abs / 'bin' / 'libstdc++-6.dll',
        )
    endif

    # Install required binaries to run project targets.
    if not fs.is_samepath(get_option('prefix') / get_option('bindir'), prebuilt_path_abs / 'bin')
      install_data(install_files, install_dir : get_option('bindir'))
    endif
  endif
endif
