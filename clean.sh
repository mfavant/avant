# 1. 获取操作系统类型
OS_NAME=$(uname -s)

cd protocol
make clean
pwd
cd ..
cd external/LuaJIT-2.1
pwd
if [ "$OS_NAME" == "Darwin" ]; then
    echo "Detected macOS, starting clean..."
    env MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion) make clean
elif [ "$OS_NAME" == "Linux" ]; then
    echo "Detected Linux, starting clean..."
    make clean
fi
cd ../../
rm -rf ./build
rm -rf ./bin/avant
