#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#include "json.hpp"
#include "http_client.hpp"
#include "http_server.hpp"
#include "webui_html.hpp"
#include "sha1.hpp"

using json = nlohmann::json;

// 单台实例的监控数据模型
struct MonitorInstance {
    std::string uuid;
    std::string alias;
    bool enabled = true;

    // 运行时监控状态
    std::string machine_id = "";
    std::string gpu_name = "";
    int current_idle_num = 0;
    int prev_idle_num = 0;
    std::string last_check_time = "-";
    std::string status_str = "未检测";
    bool has_notified_in_this_cycle = false;
};

// 全局应用配置与状态管理
class AppManager {
public:
    std::string authorization;
    std::string xizhi_url = "https://xizhi.qqoq.net/XZ92fff6ec8b293488bd67699b2d0a0faf.send";
    std::string web_host = "0.0.0.0";
    int check_interval_seconds = 30;
    int web_port = 5178;
    std::string config_path = "config.json";

    std::vector<MonitorInstance> instances;
    std::mutex mtx;

    static std::string get_current_time_str() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        return std::string(buf);
    }

    bool load_config(const std::string& path = "config.json") {
        std::lock_guard<std::mutex> lock(mtx);
        config_path = path;
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            if (j.contains("authorization") && j["authorization"].is_string()) {
                authorization = j["authorization"].get<std::string>();
            }
            if (j.contains("xizhi_url") && j["xizhi_url"].is_string()) {
                xizhi_url = j["xizhi_url"].get<std::string>();
            }
            if (j.contains("web_host") && j["web_host"].is_string()) {
                web_host = j["web_host"].get<std::string>();
            }
            if (j.contains("check_interval_seconds") && j["check_interval_seconds"].is_number()) {
                check_interval_seconds = j["check_interval_seconds"].get<int>();
            }
            if (j.contains("web_port") && j["web_port"].is_number()) {
                web_port = j["web_port"].get<int>();
            }

            instances.clear();
            if (j.contains("instances") && j["instances"].is_array()) {
                for (const auto& item : j["instances"]) {
                    MonitorInstance inst;
                    inst.uuid = item.value("uuid", "");
                    inst.alias = item.value("alias", "");
                    inst.enabled = item.value("enabled", true);
                    if (!inst.uuid.empty()) {
                        instances.push_back(inst);
                    }
                }
            } else if (j.contains("instance_uuid") && j["instance_uuid"].is_string()) {
                // 兼容旧版单实例配置
                std::string single_uuid = j["instance_uuid"].get<std::string>();
                if (!single_uuid.empty()) {
                    MonitorInstance inst;
                    inst.uuid = single_uuid;
                    inst.alias = "默认实例";
                    inst.enabled = true;
                    instances.push_back(inst);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool save_config() {
        std::lock_guard<std::mutex> lock(mtx);
        json j;
        j["authorization"] = authorization;
        j["xizhi_url"] = xizhi_url;
        j["web_host"] = web_host;
        j["check_interval_seconds"] = check_interval_seconds;
        j["web_port"] = web_port;

        json inst_arr = json::array();
        for (const auto& inst : instances) {
            json item;
            item["uuid"] = inst.uuid;
            item["alias"] = inst.alias;
            item["enabled"] = inst.enabled;
            inst_arr.push_back(item);
        }
        j["instances"] = inst_arr;

        std::ofstream file(config_path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    // Web API: 获取所有实例状态 (严格保密，不输出 Authorization)
    json get_instances_json() {
        std::lock_guard<std::mutex> lock(mtx);
        json arr = json::array();
        for (const auto& inst : instances) {
            json item;
            item["uuid"] = inst.uuid;
            item["alias"] = inst.alias;
            item["enabled"] = inst.enabled;
            item["machine_id"] = inst.machine_id;
            item["gpu_name"] = inst.gpu_name;
            item["current_idle_num"] = inst.current_idle_num;
            item["last_check_time"] = inst.last_check_time;
            item["status_str"] = inst.status_str;
            arr.push_back(item);
        }
        return arr;
    }

    // Web API: 添加新实例
    bool add_instance(const std::string& uuid, const std::string& alias) {
        if (uuid.empty()) return false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& inst : instances) {
                if (inst.uuid == uuid) {
                    inst.alias = alias;
                    inst.enabled = true;
                    return true;
                }
            }
            MonitorInstance inst;
            inst.uuid = uuid;
            inst.alias = alias.empty() ? "未命名实例" : alias;
            inst.enabled = true;
            instances.push_back(inst);
        }
        save_config();
        return true;
    }

    // Web API: 切换开启/关闭开关
    bool toggle_instance(const std::string& uuid, bool enabled) {
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& inst : instances) {
                if (inst.uuid == uuid) {
                    inst.enabled = enabled;
                    found = true;
                    break;
                }
            }
        }
        if (found) save_config();
        return found;
    }

    // Web API: 删除监控实例
    bool delete_instance(const std::string& uuid) {
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = std::remove_if(instances.begin(), instances.end(),
                                     [&uuid](const MonitorInstance& i) { return i.uuid == uuid; });
            if (it != instances.end()) {
                instances.erase(it, instances.end());
                found = true;
            }
        }
        if (found) save_config();
        return found;
    }
};

AppManager g_app;

// 发送息知推送通知
bool send_xizhi_notification(const std::string& title, const std::string& content) {
    std::string full_url = g_app.xizhi_url;
    if (full_url.find('?') == std::string::npos) {
        full_url += "?title=" + HttpClient::url_encode(title) + "&content=" + HttpClient::url_encode(content);
    } else {
        full_url += "&title=" + HttpClient::url_encode(title) + "&content=" + HttpClient::url_encode(content);
    }

    std::cout << "[推送] 正在推送有卡通知至息知微信渠道..." << std::endl;
    auto resp = HttpClient::get(full_url);
    if (resp.success) {
        std::cout << "[推送] 息知响应: " << resp.body << std::endl;
        return true;
    } else {
        std::cerr << "[推送] 发送失败: " << resp.error_message << " (状态码: " << resp.status_code << ")" << std::endl;
        return false;
    }
}

// 检查所有已启用的实例
void check_instances_task() {
    std::string auth;
    {
        std::lock_guard<std::mutex> lock(g_app.mtx);
        auth = g_app.authorization;
    }

    if (auth.empty()) return;

    // 1. 请求 AutoDL 用户实例列表 API 获取实例信息与物理机状态
    std::map<std::string, std::string> headers = {
        {"Authorization", auth},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    json req_body;
    req_body["page_index"] = 1;
    req_body["page_size"] = 100;

    auto resp = HttpClient::post_json("https://www.autodl.com/api/v1/user/instance/list",
                                      req_body.dump(), headers);

    if (!resp.success) {
        std::cerr << "[API] 获取用户实例列表失败: " << resp.error_message << std::endl;
        return;
    }

    try {
        auto res_json = json::parse(resp.body);
        if (res_json.value("code", "") == "AuthorizeFailed") {
            std::cerr << "[警告] AutoDL Authorization 已失效或超时，请在 config.json 更新 Token！" << std::endl;
            return;
        }

        if (!res_json.contains("data") || !res_json["data"].contains("list")) return;

        // 构建远端实例映射表 (uuid -> info)
        std::map<std::string, json> remote_instances;
        for (const auto& item : res_json["data"]["list"]) {
            std::string u = item.value("instance_uuid", "");
            if (!u.empty()) {
                remote_instances[u] = item;
            }
        }

        std::string now_str = AppManager::get_current_time_str();

        std::lock_guard<std::mutex> lock(g_app.mtx);
        for (auto& inst : g_app.instances) {
            // 如果用户关闭了此实例的监控开关，则直接跳过不运行
            if (!inst.enabled) {
                inst.status_str = "监控已暂停";
                continue;
            }

            auto it = remote_instances.find(inst.uuid);
            if (it == remote_instances.end()) {
                inst.last_check_time = now_str;
                inst.status_str = "未在 AutoDL 账号中找到此实例";
                continue;
            }

            const auto& info = it->second;
            inst.machine_id = info.value("machine_id", "");
            inst.gpu_name = info.value("gpu_name", "");
            inst.status_str = info.value("status", "");
            inst.last_check_time = now_str;

            // 获取该物理宿主机的空闲卡数
            int idle_count = 0;
            if (info.contains("gpu_idle_num") && info["gpu_idle_num"].is_number()) {
                idle_count = info["gpu_idle_num"].get<int>();
            } else if (info.contains("machine_gpu_idle_num") && info["machine_gpu_idle_num"].is_number()) {
                idle_count = info["machine_gpu_idle_num"].get<int>();
            } else if (info.value("status", "") == "running") {
                // 如果该实例当前本身就在运行中
                idle_count = 1;
            }

            inst.current_idle_num = idle_count;

            // ===================================================================
            // 核心状态跳变提醒逻辑 (Edge Trigger):
            // 仅在实例宿主机 GPU 从“0台”变为“>0台”时触发提醒！
            // 例子: 0 -> 1 -> 0 -> 2 -> 4 -> 2 -> 0，只在 0->1 和 0->2 触发两次提醒！
            // 绝不在有卡期间每 30 秒重复轰炸！
            // ===================================================================
            if (inst.current_idle_num > 0 && inst.prev_idle_num == 0 && !inst.has_notified_in_this_cycle) {
                std::cout << "\n======================================================\n";
                std::cout << " 🎉【检测到实例有空闲 GPU！】\n";
                std::cout << " 实例备注: " << inst.alias << "\n";
                std::cout << " 实例UUID: " << inst.uuid << "\n";
                std::cout << " GPU 型号: " << inst.gpu_name << "\n";
                std::cout << " 空闲卡数: " << inst.current_idle_num << " 台\n";
                std::cout << " 宿主机ID: " << inst.machine_id << "\n";
                std::cout << "======================================================\n";

                std::string title = "【AutoDL开机提醒】" + inst.alias + " 有空闲GPU";
                std::string content = "时间: " + now_str + "\n"
                                    + "实例备注: " + inst.alias + "\n"
                                    + "实例UUID: " + inst.uuid + "\n"
                                    + "GPU 型号: " + inst.gpu_name + "\n"
                                    + "当前空闲卡数: " + std::to_string(inst.current_idle_num) + " 台\n"
                                    + "物理宿主机: " + inst.machine_id + "\n\n"
                                    + "检测到宿主机已释放可用显卡，请立即前往控制台开机！";

                send_xizhi_notification(title, content);
                inst.has_notified_in_this_cycle = true;
            } else if (inst.current_idle_num == 0) {
                // 当卡被全部占用变为 0 时，重置周期通知标记，等待下一次释放！
                inst.has_notified_in_this_cycle = false;
            }

            // 更新上一次的历史状态
            inst.prev_idle_num = inst.current_idle_num;
        }

    } catch (const std::exception& e) {
        std::cerr << "[错误] 解析实例列表响应异常: " << e.what() << std::endl;
    }
}

// 启动 WebUI 路由服务 (端口 5178)
void setup_web_server(SimpleHttpServer& server) {
    server.set_handler([](const std::string& method,
                          const std::string& path,
                          const std::string& body,
                          std::string& resp_content_type,
                          std::string& resp_body,
                          int& resp_status) {
        // 1. 首页: 返回内嵌的 WebUI 前端页面
        if (path == "/" || path == "/index.html") {
            resp_content_type = "text/html; charset=utf-8";
            resp_body = WEBUI_HTML;
            resp_status = 200;
            return;
        }

        // 2. API: 获取监控实例列表 (严格保密，前端绝对不可见 authorization)
        if (method == "GET" && path == "/api/instances") {
            resp_content_type = "application/json; charset=utf-8";
            resp_body = g_app.get_instances_json().dump();
            resp_status = 200;
            return;
        }

        // 3. API: 添加监控实例 (传入 uuid 和 alias)
        if (method == "POST" && path == "/api/instances") {
            try {
                auto req_j = json::parse(body);
                std::string uuid = req_j.value("uuid", "");
                std::string alias = req_j.value("alias", "");
                if (g_app.add_instance(uuid, alias)) {
                    resp_content_type = "application/json; charset=utf-8";
                    resp_body = "{\"success\": true}";
                    resp_status = 200;
                } else {
                    resp_status = 400;
                    resp_body = "{\"error\": \"uuid 不能为空\"}";
                }
            } catch (...) {
                resp_status = 400;
                resp_body = "{\"error\": \"无效的 JSON\"}";
            }
            return;
        }

        // 4. API: 开启/关闭某个实例的监控开关
        if (method == "POST" && path == "/api/instances/toggle") {
            try {
                auto req_j = json::parse(body);
                std::string uuid = req_j.value("uuid", "");
                bool enabled = req_j.value("enabled", true);
                if (g_app.toggle_instance(uuid, enabled)) {
                    resp_content_type = "application/json; charset=utf-8";
                    resp_body = "{\"success\": true}";
                    resp_status = 200;
                } else {
                    resp_status = 404;
                    resp_body = "{\"error\": \"未找到对应实例\"}";
                }
            } catch (...) {
                resp_status = 400;
                resp_body = "{\"error\": \"无效的 JSON\"}";
            }
            return;
        }

        // 5. API: 删除实例
        if (method == "POST" && path == "/api/instances/delete") {
            try {
                auto req_j = json::parse(body);
                std::string uuid = req_j.value("uuid", "");
                if (g_app.delete_instance(uuid)) {
                    resp_content_type = "application/json; charset=utf-8";
                    resp_body = "{\"success\": true}";
                    resp_status = 200;
                } else {
                    resp_status = 404;
                    resp_body = "{\"error\": \"未找到对应实例\"}";
                }
            } catch (...) {
                resp_status = 400;
                resp_body = "{\"error\": \"无效的 JSON\"}";
            }
            return;
        }

        // 默认 404
        resp_status = 404;
        resp_body = "Not Found";
    });
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "======================================================\n";
    std::cout << "     AutoDL 实例 GPU 资源监控系统 (WebUI + C++ 原生版)   \n";
    std::cout << "======================================================\n\n";

    if (!g_app.load_config()) {
        std::cout << "[提示] 未检测到 config.json，正在初始化基础配置...\n";
        g_app.save_config();
    }

    // 检查并引导 authorization 凭证 (后端保密管理)
    if (g_app.authorization.empty()) {
        std::cout << "[提示] 当前未配置 AutoDL Authorization。\n";
        std::cout << "请选择授权方式:\n";
        std::cout << "  1. 粘贴 Web 控制台的 Authorization Token (推荐)\n";
        std::cout << "  2. 退出并稍后手动填入 config.json\n";
        std::cout << "请输入选项 (1 或 2): ";
        int choice = 1;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            std::cout << "请粘贴您的 Authorization Token: ";
            std::getline(std::cin, g_app.authorization);
            if (!g_app.authorization.empty()) {
                g_app.save_config();
                std::cout << "[成功] Token 已保存至 config.json！\n";
            }
        }
    }

    // 启动内嵌 WebUI 服务 (默认监听 0.0.0.0:5178)
    SimpleHttpServer web_server(g_app.web_host, g_app.web_port);
    setup_web_server(web_server);
    if (!web_server.start()) {
        std::cerr << "[WebUI] 无法开放在 " << g_app.web_host << ":" << g_app.web_port << "，请检查端口是否被占用。\n";
    }

    std::cout << "\n------------------------------------------------------\n";
    std::cout << "WebUI 控制台   : http://" << g_app.web_host << ":" << g_app.web_port << "\n";
    std::cout << "轮询检测间隔   : " << g_app.check_interval_seconds << " 秒\n";
    std::cout << "息知推送接口   : " << g_app.xizhi_url << "\n";
    std::cout << "监控状态规则   : 仅在 0->有空闲 跳变时发送一次提醒，不重复轰炸\n";
    std::cout << "------------------------------------------------------\n";
    std::cout << "[状态] 后台监控已开始运行，按 Ctrl+C 可停止程序...\n\n";

    int round = 0;
    while (true) {
        round++;
        std::string current_time = AppManager::get_current_time_str();

        int enabled_count = 0;
        int total_count = 0;
        {
            std::lock_guard<std::mutex> lock(g_app.mtx);
            total_count = static_cast<int>(g_app.instances.size());
            for (const auto& i : g_app.instances) {
                if (i.enabled) enabled_count++;
            }
        }

        std::cout << "[" << current_time << "] 第 " << round << " 次检测中 (总实例: "
                  << total_count << ", 已启用: " << enabled_count << ")..." << std::endl;

        if (enabled_count > 0) {
            check_instances_task();
        } else {
            std::cout << " -> 暂无启用的监控实例，可通过浏览器访问 http://localhost:"
                      << g_app.web_port << " 添加实例。" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(g_app.check_interval_seconds));
    }

    web_server.stop();
    return 0;
}
