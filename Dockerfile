FROM ubuntu:latest
RUN mkdir -p /avant
COPY . /avant
WORKDIR /avant
RUN apt update
RUN apt install cmake g++ make git -y
RUN apt install protobuf-compiler libprotobuf-dev  -y
RUN apt install libssl-dev -y
# AVANT_JIT_VERSION=ON
WORKDIR /avant
RUN echo "START=>building AVANT_JIT_VERSION=ON"
RUN cd protocol \
    && make clean \
    && make \
    && cd .. \
    && mkdir build \
    && rm -rf ./build/* \
    && cd build \
    && cmake -DAVANT_JIT_VERSION=ON .. \
    && make -j3 \
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
    && mkdir build \
    && rm -rf ./build/* \
    && cd build \
    && cmake -DAVANT_JIT_VERSION=OFF .. \
    && make -j3 \
    && cd .. \
    && cd bin \
    && ls
RUN echo "END=>building AVANT_JIT_VERSION=OFF"
# AVANT_JIT_VERSION=OFF
WORKDIR /avant/bin
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["./avant && tail -f /dev/null"]
