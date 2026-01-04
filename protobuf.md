# protobuf3

```bash
wget https://github.com/protocolbuffers/protobuf/releases/download/v3.11.2/protobuf-all-3.11.2.tar.gz
tar zxf protobuf-all-3.11.2.tar.gz
cd protobuf-3.11.2
./configure -prefix=/usr/local/
sudo make -j4 && sudo make install
protoc --version
```

```bash
wget https://github.com/protocolbuffers/protobuf/releases/download/v33.2/protobuf-33.2.zip
cd protobuf-33.2
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/data/home/gaowanlu/local/protobuf -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=ON -Dprotobuf_ABSL_PROVIDER=package
make -j5
make install
cd /data/home/gaowanlu/local/protobuf
ls
```
