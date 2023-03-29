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

DECLARE(`ENCODER_BUILD_SERVER',ON)

ifelse(ENCODER_BUILD_SERVER,ON,`dnl
  define(`ENCODER_BUILD_DEPS',`dnl
    cmake gcc g++ libdrm-dev dnl
    ifdef(`BUILD_ONEVPL',,libvpl-dev) dnl
    ifdef(`BUILD_LIBVA2',,libva-dev) dnl
    make patch pkg-config')

  define(`ENCODER_INSTALL_DEPS',`dnl
    ifdef(`BUILD_ONEVPL',,libvpl2) dnl
    ifdef(`BUILD_ONEVPLGPU',,libmfxgen1) dnl
    ifdef(`BUILD_LIBVA2',,libva2 libva-drm2)')
')

ifelse(ENCODER_BUILD_SERVER,OFF,`dnl
  define(`ENCODER_BUILD_DEPS',`cmake gcc g++ make pkg-config')
  define(`ENCODER_INSTALL_DEPS',`')
')

pushdef(`CFLAGS',`-D_FORTIFY_SOURCE=2 -fstack-protector-strong')

define(`BUILD_ENCODER',
COPY sources/encoder /opt/build/encoder

RUN cd BUILD_HOME/encoder \
  && mkdir _build && cd _build \
  && cmake \
    -DBUILD_SERVER=ENCODER_BUILD_SERVER \
    -DCMAKE_C_FLAGS="CFLAGS" \
    -DCMAKE_CXX_FLAGS="CFLAGS" \
    -DCMAKE_INSTALL_PREFIX=BUILD_PREFIX \
    -DCMAKE_INSTALL_LIBDIR=BUILD_LIBDIR \
    .. \
  && make VERBOSE=1 -j $(nproc --all) \
  && make install DESTDIR=BUILD_DESTDIR \
  && make install
) dnl define(BUILD_ENCODER)

popdef(`CFLAGS')

REG(ENCODER)

include(end.m4)
