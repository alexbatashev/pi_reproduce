FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -yqq \
     build-essential \
     wget \
     ninja-build \
     software-properties-common \
     python3 \
     git && apt clean
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt update && apt install -yqq gcc-11 g++-11 && apt clean
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 --slave \
     /usr/bin/g++ g++ /usr/bin/g++-11 --slave /usr/bin/gcov gcov /usr/bin/gcov-11

RUN wget https://github.com/Kitware/CMake/releases/download/v3.21.1/cmake-3.21.1-linux-x86_64.sh \
  && bash cmake-3.21.1-linux-x86_64.sh --skip-license && rm cmake-3.21.1-linux-x86_64.sh

WORKDIR /src

ARG LLVM_TAG=xpti_global_objects_revisited

RUN git clone --depth 1 --branch $LLVM_TAG https://github.com/alexbatashev/llvm.git
RUN cd llvm && mkdir build && cd build && \
      python3 ../buildbot/configure.py --no-werror \
      --llvm-external-projects="lldb" \
      --cmake-opt="-DCMAKE_INSTALL_PREFIX=/src/llvm_install" \
      && ninja sycl-toolchain lldb && ninja install && \
      cp lib/libxpti* /src/llvm_install/lib && cp lib/liblldb* /src/llvm_install/lib &&\
      cd .. && rm -rf build

ENTRYPOINT [ /bin/bash ]
