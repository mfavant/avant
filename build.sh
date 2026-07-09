make
cd protocol && make
cd ..
mkdir -p build
cd build
cmake -DAVANT_JIT_VERSION=ON ..
make -j$(nproc)
cd ..
pwd
