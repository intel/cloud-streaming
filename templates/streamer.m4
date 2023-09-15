dnl BSD 3-Clause License
dnl
dnl Copyright (C) 2020-2022, Intel Corporation
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

include(sioclient.m4)
define(`STREAMER_BUILD_DEPS',gcc g++ dnl
  libssl-dev libgtest-dev meson nlohmann-json3-dev pkg-config)
define(`STREAMER_INSTALL_DEPS',dnl
  openssl)

pushdef(`CFLAGS',`-D_FORTIFY_SOURCE=2 -fstack-protector-strong')

define(`BUILD_STREAMER',
COPY . /opt/build/streamer

ENV GAPATH=BUILD_HOME
RUN cd BUILD_HOME/streamer && \
  meson setup \
    --buildtype=release \
    --prefix=BUILD_PREFIX \
    --libdir=BUILD_LIBDIR \
    --wrap-mode=nofallback \
    -Dc_args="CFLAGS" \
    -Dcpp_args="CFLAGS" \
    -Dm4=disabled \
    -Dencoder=disabled \
    -Dstreamer=enabled \
    _build && \
  meson compile -C _build -v && \
  meson install -C _build && \
  meson install -C _build --destdir=BUILD_DESTDIR
) dnl define(BUILD_STREAMER)

popdef(`CFLAGS')

REG(STREAMER)

include(end.m4)
