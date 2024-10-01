FROM ubuntu:latest
RUN mkdir -p /avant
COPY . /avant
WORKDIR /avant
RUN apt-get update && apt-get install -y apt-utils
RUN apt-get install cmake g++ make git -y
RUN apt-get install protobuf-compiler libprotobuf-dev  -y
RUN apt-get install libssl-dev -y
WORKDIR /avant
RUN rm -rf CMakeCache.txt \
    && cd protocol \
    && make \
    && cd .. \
    && mkdir build \
    && rm -rf ./build/* \
    && cd build \
    && cmake .. \
    && make -j3 \
    && cd .. \
    && cd bin \
    && ls
WORKDIR /avant/bin
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["./avant && tail -f /dev/null"]
