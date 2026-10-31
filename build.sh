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
    echo "[错误] 未检测到 g++ 或 clang++ 编译器。"
    echo "请先安装编译器，例如: sudo apt-get install -y g++"
    exit 1
fi

echo "使用编译器: $CXX"

# 直接使用 g++ 编译，零外部库依赖，无需 -lcurl
$CXX -std=c++17 -O2 main.cpp -o autodl_monitor

if [ $? -eq 0 ]; then
    chmod +x autodl_monitor
    echo ""
    echo "[成功] 编译完成，生成可执行文件: ./autodl_monitor"
    echo "运行命令: ./autodl_monitor"
else
    echo ""
    echo "[失败] 编译失败，请检查编译日志。"
    exit 1
fi
