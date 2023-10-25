dnl BSD 3-Clause License
dnl
dnl Copyright (C) 2020-2023, Intel Corporation
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are met:
dnl
dnl * Redistributions of source code must retain the above copyright notice, this
dnl   list of conditions and the following disclaimer.
dnl
dnl * Redistributions in binary form must reproduce the above copyright notice,
dnl   this list of conditions and the following disclaimer in the documentation
dnl   and/or other materials provided with the distribution.
dnl
dnl * Neither the name of the copyright holder nor the names of its
dnl   contributors may be used to endorse or promote products derived from
dnl   this software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
dnl AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
dnl CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
include(begin.m4)

DECLARE(`OWT_GS_VER',9ec5c2f) # tracking main branch
DECLARE(`OWT_GS_SRC_REPO',https://github.com/open-webrtc-toolkit/owt-client-native.git)
DECLARE(`DEPOT_TOOLS_VER',5b0c934) # tracking main branch

include(git.m4)

define(`OWT_GS_BUILD_DEPS',ca-certificates curl gcc g++ git lbzip2 libglib2.0-dev pkg-config python3 python3-setuptools xz-utils)
define(`OWT_GS_INSTALL_DEPS',)

define(`BUILD_OWT_GS',`dnl
ARG OWT_GS_REPO=OWT_GS_SRC_REPO
RUN mkdir -p BUILD_HOME/owt-gs && cd BUILD_HOME/owt-gs && \
  git clone $OWT_GS_REPO src && cd src && \
  git checkout OWT_GS_VER

ARG DEPOT_TOOLS_REPO=https://chromium.googlesource.com/chromium/tools/depot_tools.git
RUN cd BUILD_HOME/owt-gs && \
  git clone $DEPOT_TOOLS_REPO depot_tools && cd depot_tools && \
  git checkout DEPOT_TOOLS_VER

RUN mkdir -p BUILD_HOME/owt-gs && { \
  echo "solutions = ["; \
  echo "{"; \
  echo "   \"managed\": False,"; \
  echo "   \"name\": \"src\","; \
  echo "   \"url\": \"https://github.com/open-webrtc-toolkit/owt-client-native.git\","; \
  echo "   \"custom_deps\": {},"; \
  echo "   \"deps_file\": \"DEPS\","; \
  echo "   \"safesync_url\": \"\","; \
  echo "   \"custom_vars\": {\"checkout_instrumented_libraries\": False},"; \
  echo "},"; \
  echo "]"; \
  echo "target_os = []"; \
} > BUILD_HOME/owt-gs/.gclient
RUN { \
  echo "[Boto]"; \
  echo "proxy = $(echo $http_proxy | sed -E "s%http://(.*):(.*)$%\1%")"; \
  echo "proxy_port = $(echo $http_proxy | sed -E "s%http://(.*):(.*)$%\2%")"; \
  echo "https_validate_certificate = True"; \
} > BUILD_HOME/owt-gs/.boto && cat BUILD_HOME/owt-gs/.boto

RUN cd BUILD_HOME/owt-gs && \
  export PATH=BUILD_HOME/owt-gs/depot_tools:$PATH && \
  export NO_AUTH_BOTO_CONFIG=$(pwd)/.boto && \
  export DEPOT_TOOLS_UPDATE=0 && \
  gclient sync --no-history

RUN cd BUILD_HOME/owt-gs/src && mkdir dist && \
  export PATH=BUILD_HOME/owt-gs/depot_tools:$PATH && \
  python3 scripts/build_linux.py --gn_gen --sdk --cloud_gaming --output_path dist \
    --scheme release --arch x64 --use_gcc --shared --fake_audio

RUN cd BUILD_HOME/owt-gs/src && \
  mkdir -p BUILD_PREFIX/include && mkdir -p BUILD_LIBDIR && \
  mkdir -p BUILD_DESTDIR/BUILD_PREFIX/include && mkdir -p BUILD_DESTDIR/BUILD_LIBDIR && \
  cp -rd dist/include BUILD_PREFIX && \
  cp -rd dist/include BUILD_DESTDIR/BUILD_PREFIX && \
  cp dist/libs/* BUILD_LIBDIR && \
  cp dist/libs/* BUILD_DESTDIR/BUILD_LIBDIR

ifdef(`BUILD_REDIST',
RUN mkdir -p BUILD_REDIST/linux-x86_64/BUILD_PREFIX/include && \
  mkdir -p BUILD_REDIST/linux-x86_64/BUILD_LIBDIR/ && \
  cd BUILD_HOME/owt-gs/src && \
  cp -rd dist/include BUILD_REDIST/linux-x86_64/BUILD_PREFIX && \
  cp dist/libs/* BUILD_REDIST/linux-x86_64/BUILD_LIBDIR && \
  cd BUILD_REDIST/ && \
  tar cJvf owt.tar.xz linux-x86_64
)dnl
') # define(BUILD_OWT_GS)

REG(OWT_GS)

include(end.m4)
