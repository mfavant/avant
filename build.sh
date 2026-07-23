# 1. 获取操作系统类型
OS_NAME=$(uname -s)

# 2. 根据系统设置正确的 CPU 核心数变量（解决 macOS 没有 nproc 的问题）
if [ "$OS_NAME" == "Darwin" ]; then
    JOBS=$(sysctl -n hw.logicalcpu)
elif [ "$OS_NAME" == "Linux" ]; then
    JOBS=$(nproc)
else
    echo "Unsupported OS: $OS_NAME"
    exit 1
fi

make
cd protocol && make
cd ..

cd external/LuaJIT-2.1.ROLLING
pwd
# 3. 根据系统执行不同的 make 命令
if [ "$OS_NAME" == "Darwin" ]; then
    echo "Detected macOS, starting clean..."
    env MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion) make clean
    env MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion) make -j$JOBS
elif [ "$OS_NAME" == "Linux" ]; then
    echo "Detected Linux, starting clean..."
    make clean
    make -j$JOBS
fi

cd ../../
mkdir -p build
cd build
cmake -DAVANT_JIT_VERSION=ON ..
# 这里统一使用上面获取的 JOBS 变量，避免在 macOS 报错
make -j$JOBS
cd ..
pwd
