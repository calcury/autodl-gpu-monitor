@echo off
chcp 65001 > nul
echo 正在编译 AutoDL 实例 GPU 监控系统 (WebUI + C++ 原生版)...
g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lws2_32 -lwinhttp
if %ERRORLEVEL% EQU 0 (
    echo [成功] 编译完成，生成可执行文件: autodl_monitor.exe
    echo 启动后可直接在浏览器打开: http://localhost:5178
) else (
    echo [失败] 编译出错，请检查 g++ 环境。
)
pause
