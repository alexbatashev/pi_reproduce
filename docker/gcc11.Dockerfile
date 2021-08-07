FROM ubuntu:21.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -yqq \
     build-essential \
     cmake \
     ninja-build \
     python3 \
     git \
     libboost-all-dev \
     libevent-dev \
     libdouble-conversion-dev \
     libgoogle-glog-dev \
     libgflags-dev \
     libiberty-dev \
     liblz4-dev \
     liblzma-dev \
     libsnappy-dev \
     zlib1g-dev \
     binutils-dev \
     libjemalloc-dev \
     libssl-dev \
     pkg-config \
     libunwind-dev

WORKDIR /src

ARG LLVM_TAG=sycl

RUN git clone https://github.com/fmtlib/fmt.git && cd fmt && mkdir __build && \
      cd __build && cmake -GNinja .. && ninja install && cd ../../ && rm -rf fmt
RUN git clone --depth 1 --branch cache_locality_cpp20 https://github.com/alexbatashev/folly.git \
      && cd folly && mkdir __build && cd __build && cmake -GNinja .. && \
      ninja && ninja install && cd ../../ && rm -rf folly

RUN git clone --depth 1 --branch $LLVM_TAG https://github.com/intel/llvm.git
RUN cd llvm && mkdir build && cd build && \
      python3 ../buildbot/configure.py --no-werror \
      --llvm-external-projects="lldb" \
      --cmake-opt="-DCMAKE_INSTALL_PREFIX=/src/llvm_install" \
      && ninja sycl-toolchain lldb && ninja install && \
      cp lib/libxptifw.so /src/llvm_install/lib && cd .. && rm -rf build

ENTRYPOINT [ /bin/bash ]
