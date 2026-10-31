@echo off
chcp 65001 > nul
echo 正在编译 AutoDL GPU 监控程序 (C++ 版本)...
g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lwinhttp
if %ERRORLEVEL% EQU 0 (
    echo [成功] 编译完成，生成可执行文件: autodl_monitor.exe
) else (
    echo [失败] 编译出错，请检查 g++ 编译环境。
)
pause
