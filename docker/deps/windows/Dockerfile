ARG IMAGE=ubuntu:23.04
FROM $IMAGE

RUN mkdir -p /opt/build && mkdir -p /opt/dist

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    git \
    make \
    mingw-w64 \
    mingw-w64-tools \
    nasm \
    patch \
    perl \
    unzip \
    wget \
    && \
  rm -rf /var/lib/apt/lists/*

# NOTE:
# * Using 'install_sw' to avoid installing documentation which we don't need
RUN cd /opt/build && \
  git clone https://github.com/openssl/openssl.git && cd openssl && \
  git checkout openssl-3.0.10 && \
  perl Configure \
    --prefix=/opt/dist/build-deps/windows-x86_64 \
    --libdir=/opt/dist/build-deps/windows-x86_64/lib \
    --openssldir=/opt/dist/build-deps/windows-x86_64 \
    --cross-compile-prefix=x86_64-w64-mingw32- \
    shared mingw64 && \
  make -j$(nproc) && make install_sw

RUN cd /opt/build && \
  git clone https://github.com/intel/libvpl.git libvpl && cd libvpl && \
  git checkout v2023.3.1 && \
  cmake -B _build \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_INSTALL_PREFIX=/opt/dist/build-deps/windows-x86_64 \
    -DBUILD_TOOLS=OFF \
    -DBUILD_PREVIEW=OFF \
    -DINSTALL_EXAMPLE_CODE=OFF \
    && \
  make -j$(nproc) -C _build && \
  make install -C _build

ENV PKG_CONFIG_PATH=/opt/dist/build-deps/windows-x86_64/lib/pkgconfig

# d3d11va AV1 decoder requires mingw >= 10.x (Ubuntu 22.10 and later)
RUN cd /opt/build && \
  wget -O FFmpeg-n6.1.zip https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n6.1.zip && \
  unzip FFmpeg-n6.1.zip && cd FFmpeg-n6.1 && \
  ./configure \
    --enable-cross-compile \
    --arch=x86_64 \
    --target-os=mingw64 \
    --cross-prefix=x86_64-w64-mingw32- \
    --disable-doc \
    --enable-shared \
    --enable-libvpl \
    --prefix=/opt/dist/build-deps/windows-x86_64 && \
  make -j$(nproc) && \
  make install

RUN cd /opt/dist/build-deps/windows-x86_64/bin && \
  cp /usr/lib/gcc/x86_64-w64-mingw32/12-win32/libgcc_s_seh-1.dll . && \
  cp /usr/lib/gcc/x86_64-w64-mingw32/12-win32/libstdc++-6.dll .
