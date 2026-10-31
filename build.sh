#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "    正在编译 AutoDL 实例 GPU 监控系统 (Linux 原生版)     "
echo "=========================================================="

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

# 零外部依赖编译，不需要安装 libcurl 开发包，仅链接标准 pthread
$CXX -std=c++17 -O2 main.cpp -o autodl_monitor -lpthread

if [ $? -eq 0 ]; then
    chmod +x autodl_monitor
    echo ""
    echo "[成功] 编译完成，生成可执行文件: ./autodl_monitor"
    echo "启动后浏览器访问: http://localhost:5178"
else
    echo ""
    echo "[失败] 编译失败，请检查编译输出日志。"
    exit 1
fi
