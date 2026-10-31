# AutoDL 实例 GPU 监控系统 (WebUI + C++ 原生跨平台版)

> 🚀 专为 AutoDL 关机实例“开机抢卡”打造的原生守护工具（**支持 WebUI 界面、多实例独立监控、跳变提醒、Windows & Linux 零依赖编译**）

当你按量计费的 AutoDL 实例关机后，显卡会被释放。当你想重新开机时，若宿主机上的 GPU 被占满，往往提示“宿主机 GPU 资源不足，无法开机”。本项目专为解决该痛点而设计，全天候守护你的指定实例，并在**宿主机有空闲卡释放的瞬间**立即通过**息知**微信公众号推送开机通知！

---

## ✨ 核心特性

1. 🖥️ **内置轻量 WebUI 控制台 (开放在 5178 端口)**：
   - 浏览器直接访问 `http://localhost:5178`。
   - 界面可视化添加/删除监控实例（输入 UUID 和备注名称）。
   - 每一台实例均提供**独立的开启/关闭监控开关**，关闭后立即停止检测。
   - **严格隐私安全**：WebUI 界面与 API **绝不泄露与返回 Authorization Token**，对前端彻底保密。
2. 🎯 **多实例独立监控与详细备注**：
   - 支持同时监控任意多台不同的实例，各自具有独立备注（例如 `4090论文训练机`、`A100测试机`）。
   - 微信推送消息中明确指出**具体是哪一台机器释放了显卡**（附带自定义备注、UUID、GPU 型号、空闲卡数及物理机 ID）。
3. 🔔 **智能状态跳变提醒 (杜绝频繁骚扰)**：
   - **核心判定机制**：仅在实例 GPU 从“无空闲（0台）”跃迁为“有空闲（>=1台）”的跳变瞬间触发微信提醒！
   - **示例**：空闲数在 `0 -> 1 -> 0 -> 2 -> 4 -> 2 -> 0` 变化时，**仅在 `0 -> 1` 和 `0 -> 2` 触发两次提醒**！在有卡期间（1, 2, 4 等）绝不会每隔 30 秒重复轰炸。
4. ⚡ **纯 C++ 零环境依赖构建 (无需 CMake)**：
   - **Linux**：无需安装 `libcurl4-openssl-dev` 或任何外部开发库，单行裸 `g++` 即可直接编译出原生 ELF 单文件！
   - **Windows**：基于系统内置 `WinHTTP` 与 `Winsock`，单行命令生成单一 `.exe`。

---

## 🛠️ 编译说明（无需 CMake，零外部依赖）

### 🐧 Linux 环境
无需安装任何额外包，直接在项目目录执行：
```bash
g++ -std=c++17 -O2 main.cpp -o autodl_monitor -lpthread
```
或者运行脚本：
```bash
chmod +x build.sh
./build.sh
```

### 🪟 Windows 环境
- **命令行**：
  ```cmd
  g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lws2_32 -lwinhttp
  ```
- **双击脚本**：直接双击运行 [`build.bat`](file:///e:/github/collaborate/autodl-gpu-monitor/build.bat)。

---

## 🌐 WebUI 界面使用 (监听 0.0.0.0:5178)

启动程序后，服务会监听在 **`0.0.0.0:5178`**：
- **本机访问**：👉 `http://localhost:5178` 或 `http://127.0.0.1:5178`
- **局域网/公网访问**：👉 `http://<你的服务器IP>:5178`（在云服务器或局域网机器上挂机时，可以直接用手机或另一台电脑远程管理）

- **查看状态**：实时查看每台实例的空闲 GPU 卡数、物理宿主机编号、最后检测时间。
- **添加实例**：点击右上角“+ 添加监控实例”，输入从 AutoDL 控制台复制的实例 UUID 和自定义备注。
- **独立开关**：随时点击滑动开关开启或暂停某台实例的监控（修改即时生效并同步保存至 `config.json`）。
- **删除监控**：点击卡片底部的“删除监控”按钮即可移除。

---

## ⚙️ 配置文件说明 (`config.json`)

配置文件自动与 WebUI 保持双向同步：

```json
{
  "authorization": "你的AutoDL Token",
  "xizhi_url": "https://xizhi.qqoq.net/XZ92fff6ec8b293488bd67699b2d0a0faf.send",
  "check_interval_seconds": 30,
  "web_port": 5178,
  "instances": [
    {
      "uuid": "0a1b2c3d-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
      "alias": "4090大模型微调",
      "enabled": true
    },
    {
      "uuid": "9f8e7d6c-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
      "alias": "A100实验机",
      "enabled": false
    }
  ]
}
```

---

## 🚀 启动与后台常驻

### 前台启动
- **Linux**: `./autodl_monitor`
- **Windows**: `autodl_monitor.exe`

### Linux 服务器后台静默守护
```bash
nohup ./autodl_monitor > monitor.log 2>&1 &

# 查看日志
tail -f monitor.log

# 停止程序
pkill autodl_monitor
```
