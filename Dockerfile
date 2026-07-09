FROM ubuntu:latest
RUN mkdir -p /avant
COPY . /avant
WORKDIR /avant
RUN apt update
RUN apt install cmake g++ make git nodejs npm -y
RUN apt install protobuf-compiler libprotobuf-dev  -y
RUN apt install libssl-dev -y
# AVANT_JIT_VERSION=ON
WORKDIR /avant
RUN echo "START=>building AVANT_JIT_VERSION=ON"
RUN cd external/LuaJIT-2.1.ROLLING \
    && make clean \
    && make -j$(nproc)

# if macos
# RUN cd external/LuaJIT-2.1.ROLLING \
#     && make clean \
#     && env MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion) make -j$(nproc)

WORKDIR /avant
RUN cd protocol \
    && make clean \
    && make \
    && cd .. \
    && mkdir -p build \
    && rm -rf ./build/* \
    && cd build \
    && cmake -DAVANT_JIT_VERSION=ON .. \
    && make -j$(nproc) \
    && cd .. \
    && cd bin \
    && ls
RUN echo "END=>building AVANT_JIT_VERSION=ON"
# AVANT_JIT_VERSION=OFF
WORKDIR /avant
RUN echo "START=>building AVANT_JIT_VERSION=OFF"
RUN cd protocol \
    && make clean \
    && make \
    && cd .. \
    && mkdir -p build \
    && rm -rf ./build/* \
    && cd build \
    && cmake -DAVANT_JIT_VERSION=OFF .. \
    && make -j$(nproc) \
    && cd .. \
    && cd bin \
    && ls
RUN echo "END=>building AVANT_JIT_VERSION=OFF"
# AVANT_JIT_VERSION=OFF

WORKDIR /avant
RUN make clean && make

WORKDIR /avant/bin
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["./avant && tail -f /dev/null"]
