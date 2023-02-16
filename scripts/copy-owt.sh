#!/bin/bash

set -ex

docker run --rm -u $(id -u):$(id -g) \
  -v $(pwd)/prebuilt:/opt/prebuilt \
  $1 \
  /bin/bash -c "cp owt.tar.xz /opt/prebuilt/"

cd prebuilt && tar xJvf owt.tar.xz && rm -f owt.tar.xz

