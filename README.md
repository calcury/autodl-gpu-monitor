# AutoDL GPU 自动监控工具 (C++ 跨平台版)

> 🚀 完全使用 C++ 重构的原生 GPU 资源监控与推送工具（**支持 Windows & Linux，无需 CMake**，无需 Python 环境，单命令行/脚本一键编译）

本项目专为 AutoDL 平台打造，纯 C++ 编写，毫秒级网络 I/O。专注于**监控目标 GPU 空闲资源**，并在检测到有可用算力时，自动通过**息知 (xizhi)** 渠道推送到微信。

---

## ✨ 核心特性

- ⚡ **纯 C++ 跨平台构建**：
  - **Windows**：基于系统内置 `WinHTTP`，**零外部依赖**，直接编译生成 `.exe`。
  - **Linux**：基于标准 `libcurl`，兼顾稳定与极低延迟，**单行命令编译生成原生 ELF 二进制**。
- 📦 **不需要 CMake**：无论在 Windows 还是 Linux，均只需一行 `g++` 命令（或直接运行脚本）即可完成编译。
- 🛡️ **安全纯净**：已彻底剔除下单抢购与自动扣费代码，仅保留 **检测 + 推送**，无误扣款风险。
- 📱 **即时推送**：集成**息知**微信公众号推送，第一时间获取有卡通知。
- 💾 **自动持久化**：首次启动交互式引导输入 Token 或登录，自动保存到 `config.json`。

---

## 🛠️ 编译说明（无需 CMake）

### 🐧 Linux 环境

#### 1. 前置依赖（标准编译链与 libcurl）
```bash
# Ubuntu / Debian
sudo apt-get update && sudo apt-get install -y build-essential libcurl4-openssl-dev

# CentOS / RHEL
sudo yum install -y gcc-c++ libcurl-devel

# Arch Linux
sudo pacman -S gcc curl

# Alpine Linux
apk add g++ curl-dev
```

#### 2. 一键编译
- **方式一（脚本）**：
  ```bash
  chmod +x build.sh
  ./build.sh
  ```
- **方式二（单行命令）**：
  ```bash
  g++ -std=c++17 -O2 main.cpp -o autodl_monitor -lcurl -lpthread
  ```

---

### 🪟 Windows 环境

本地安装有 `g++`（MinGW-w64 / w64devkit）即可：
- **方式一**：直接双击运行 [`build.bat`](file:///e:/github/collaborate/autodl-gpu-monitor/build.bat)
- **方式二**：命令行编译：
  ```cmd
  g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lwinhttp
  ```

---

## ⚙️ 配置文件说明 (`config.json`)

```json
{
  "authorization": "",
  "gpu_model": "RTX 4090",
  "check_interval_seconds": 30,
  "xizhi_url": "https://xizhi.qqoq.net/XZ92fff6ec8b293488bd67699b2d0a0faf.send"
}
```

- **authorization**: AutoDL 的授权令牌（可留空，程序首次运行会引导粘贴或登录，并自动写回）。
- **gpu_model**: 需要监控的 GPU 型号，如 `"RTX 4090"`, `"RTX 5090"`, `"A100-SXM4-80GB"`, `"RTX 3090"` 等。
- **check_interval_seconds**: 轮询检测间隔（秒），默认 30 秒。
- **xizhi_url**: 息知消息推送接口。

---

## 🚀 运行方法

### 前台运行
- **Linux**:
  ```bash
  ./autodl_monitor
  ```
- **Windows**:
  ```cmd
  autodl_monitor.exe
  ```

### Linux 后台静默运行（长期挂机监控）
可以使用 `nohup` 让程序在退出终端后依然在后台持续守护：
```bash
nohup ./autodl_monitor > monitor.log 2>&1 &

# 查看实时运行日志
tail -f monitor.log

# 停止后台监控
pkill autodl_monitor
```
