# AutoDL GPU 自动监控工具 (C++ 零依赖原生跨平台版)

> 🚀 完全使用 C++ 重构的原生 GPU 资源监控与推送工具（**支持 Windows & Linux，零环境依赖，无需 CMake**，无需 Python 环境，使用 `g++` 直接一键编译）

本项目专为 AutoDL 平台打造，纯 C++ 编写。专注于**监控目标 GPU 空闲资源**，并在检测到有可用算力时，自动通过**息知 (xizhi)** 渠道推送到微信。

---

## ✨ 核心特性

- ⚡ **纯 C++ 零环境依赖构建**：
  - **Linux**：**彻底移除 `<curl/curl.h>` 和 `-lcurl` 依赖**，不需要任何 `sudo apt install libcurl4-openssl-dev` 或开发库，所有代码保存在目录中，只需一个裸 `g++` 即可直接编译出独立可执行文件！
  - **Windows**：基于系统内置 `WinHTTP`，零外部依赖，一行命令或双击批处理即可生成 `.exe`。
- 📦 **不需要 CMake**：单行 `g++` 命令直接编译完成。
- 🛡️ **安全纯净**：已彻底剔除下单抢购与自动扣费代码，仅保留 **检测 + 推送**，无误扣款风险。
- 📱 **即时推送**：集成**息知**微信公众号推送，第一时间获取有卡通知。
- 💾 **自动持久化**：首次启动交互式引导输入 Token 或登录，自动保存到 `config.json`。

---

## 🛠️ 编译说明（零环境依赖，无需 CMake）

### 🐧 Linux 环境

**无需安装任何额外开发库**，直接在项目目录执行：

```bash
g++ -std=c++17 -O2 main.cpp -o autodl_monitor
```
或者运行脚本：
```bash
chmod +x build.sh
./build.sh
```

> 💡 彻底消除了 `curl/curl.h: No such file or directory` 问题，项目内所有依赖（JSON 处理、SHA-1 计算、HTTP 客户端）全部自包含在工程目录下。

---

### 🪟 Windows 环境

- **命令行编译**：
  ```cmd
  g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lwinhttp
  ```
- **双击脚本**：直接运行根目录下的 [`build.bat`](file:///e:/github/collaborate/autodl-gpu-monitor/build.bat)。

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
```bash
nohup ./autodl_monitor > monitor.log 2>&1 &

# 查看实时日志
tail -f monitor.log

# 停止监控
pkill autodl_monitor
```
