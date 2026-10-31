#!/usr/bin/env bash
set -e

echo "===================================================="
echo "    正在编译 AutoDL GPU 监控程序 (Linux 版本)       "
echo "===================================================="

# 检测 C++ 编译器
if command -v g++ >/dev/null 2>&1; then
    CXX="g++"
elif command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
else
    echo "[错误] 未检测到 C++ 编译器 (g++ 或 clang++)。"
    echo "请先安装编译器，例如："
    echo "  Ubuntu/Debian: sudo apt-get update && sudo apt-get install -y build-essential libcurl4-openssl-dev"
    echo "  CentOS/RHEL:   sudo yum install -y gcc-c++ libcurl-devel"
    echo "  Arch Linux:    sudo pacman -S gcc curl"
    echo "  Alpine Linux:  apk add g++ curl-dev"
    exit 1
fi

echo "使用编译器: $CXX"

# 执行编译 (无需 CMake，单命令编译)
$CXX -std=c++17 -O2 main.cpp -o autodl_monitor -lcurl -lpthread

if [ $? -eq 0 ]; then
    chmod +x autodl_monitor
    echo ""
    echo "[成功] 编译完成，生成可执行文件: ./autodl_monitor"
    echo "运行命令: ./autodl_monitor"
else
    echo ""
    echo "[失败] 编译失败！如果提示找不到 curl/curl.h 或 -lcurl，请安装 libcurl 开发库："
    echo "  Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev"
    echo "  CentOS/RHEL:   sudo yum install libcurl-devel"
    exit 1
fi
