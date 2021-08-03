FROM ubuntu:21.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -yqq \
     build-essential \
     cmake \
     ninja-build \
     python3 \
     git

WORKDIR /src

ARG LLVM_TAG=sycl

RUN git clone --depth 1 --branch $LLVM_TAG https://github.com/intel/llvm.git
RUN cd llvm && mkdir build && cd build && \
      python3 ../buildbot/configure.py --no-werror \
      --llvm-external-projects="lldb" \
      --cmake-opt="-DCMAKE_INSTALL_PREFIX=/src/llvm_install" \
      && ninja sycl-toolchain lldb && ninja install && \
      cp lib/libxptifw.so /src/llvm_install/lib && cd .. && rm -rf build

ENTRYPOINT [ /bin/bash ]
