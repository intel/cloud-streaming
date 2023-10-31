ARG IMAGE=ubuntu:23.04
FROM $IMAGE

RUN mkdir -p /opt/build && mkdir -p /opt/dist

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
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

# d3d11va AV1 decoder requires mingw >= 10.x (Ubuntu 22.10 and later)
RUN cd /opt/build && \
  wget -O FFmpeg-n6.0.zip https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n6.0.zip && \
  unzip FFmpeg-n6.0.zip && cd FFmpeg-n6.0 && \
  ./configure \
    --enable-cross-compile \
    --arch=x86_64 \
    --target-os=mingw64 \
    --cross-prefix=x86_64-w64-mingw32- \
    --disable-doc \
    --enable-shared \
    --prefix=/opt/dist/build-deps/windows-x86_64 && \
  make -j$(nproc) && \
  make install
