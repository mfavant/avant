#!/bin/bash

# 检查 protoc 是否安装
if ! command -v protoc >/dev/null 2>&1; then
    echo "❌ 未找到 protoc，请先安装 Protocol Buffers 编译器 (protoc)"
    exit 1
fi

echo "✅ 检测到 protoc 版本: $(protoc --version)"

# 安装 protoc-gen-go 插件
echo "📦 安装 protoc-gen-go ..."
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
# 立即添加 $GOPATH/bin 到 PATH，确保 protoc 能找到插件
export PATH=$PATH:$(go env GOPATH)/bin

# proto生成go代码
echo "📄 生成 protobuf Go 代码..."
mkdir -p proto_res
protoc --proto_path=../../../protocol --go_out=./ ../../../protocol/*.proto

# 创建 bin 目录
mkdir -p bin

# 编译 Go 项目
echo "🚧 构建 Go 项目..."
go build -o bin/avant

if [ $? -eq 0 ]; then
    echo "✅ 构建成功，输出文件: bin/avant"
else
    echo "❌ 构建失败"
    exit 1
fi
