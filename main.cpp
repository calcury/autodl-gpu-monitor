#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

#include "json.hpp"
#include "http_client.hpp"
#include "sha1.hpp"

using json = nlohmann::json;

// 区域友好中文名称映射
const std::map<std::string, std::string> REGION_NAMES = {
    {"[\"nm-B1\",\"nm-B2\"]", "内蒙B区"},
    {"[\"cq-A1\"]", "重庆A区"},
    {"[\"west-B\",\"west-C\"]", "西北B区"},
    {"[\"bj-B1\"]", "北京B区"},
    {"[\"beijing-A\",\"beijing-B\",\"beijing-D\",\"beijing-E\"]", "北京A区"},
    {"[\"foshan-A\"]", "佛山区"},
    {"[\"neimeng-A\",\"neimeng-C\",\"neimeng-D\"]", "内蒙A区"},
    {"[\"bj-C1\"]", "L20专区"},
    {"[\"yz-A1\"]", "3090专区"},
    {"[\"beijing-C\"]", "V100专区"},
    {"[\"nm-A1\"]", "A800专区"},
    {"[\"gd-A1\"]", "华为昇腾专区"}
};

// GPU 型号与支持区域映射
const std::map<std::string, std::vector<std::vector<std::string>>> GPU_TO_REGIONS = {
    {"RTX 5090", {{"west-B", "west-C"}}},
    {"RTX 4090", {{"nm-B1", "nm-B2"}, {"cq-A1"}, {"west-B", "west-C"}, {"bj-B1"}}},
    {"RTX 4090D", {{"cq-A1"}, {"west-B", "west-C"}}},
    {"RTX 3090", {{"nm-B1", "nm-B2"}, {"cq-A1"}, {"west-B", "west-C"}, {"beijing-A", "beijing-B", "beijing-D", "beijing-E"}, {"yz-A1"}}},
    {"RTX 3080*2", {{"nm-B1", "nm-B2"}, {"west-B", "west-C"}}},
    {"RTX 3080 Ti", {{"west-B", "west-C"}}},
    {"RTX 3080", {{"beijing-A", "beijing-B", "beijing-D", "beijing-E"}}},
    {"RTX 3060", {{"west-B", "west-C"}}},
    {"RTX 2080 Ti *2", {{"nm-B1", "nm-B2"}, {"cq-A1"}}},
    {"RTX 2080 Ti", {{"beijing-A", "beijing-B", "beijing-D", "beijing-E"}}},
    {"GTX 1080 Ti", {{"west-B", "west-C"}}},
    {"vGPU-32GB", {{"nm-B1", "nm-B2"}, {"west-B", "west-C"}}},
    {"vGPU-48GB", {{"west-B", "west-C"}}},
    {"V100-32GB", {{"foshan-A"}, {"beijing-C"}}},
    {"V100-SXM2-32GB", {{"beijing-A", "beijing-B", "beijing-D", "beijing-E"}}},
    {"A100-PCIE-40GB", {{"foshan-A"}}},
    {"A100-SXM4-80GB", {{"neimeng-A", "neimeng-C", "neimeng-D"}}},
    {"L20", {{"bj-C1"}}},
    {"L40", {{"west-B", "west-C"}}},
    {"A40", {{"neimeng-A", "neimeng-C", "neimeng-D"}}},
    {"H800", {{"west-B", "west-C"}}},
    {"A800-80GB", {{"nm-A1"}}},
    {"H20-NVLink", {{"beijing-A", "beijing-B", "beijing-D", "beijing-E"}, {"bj-C1"}}},
    {"TITAN Xp", {{"nm-B1", "nm-B2"}}},
    {"Tesla T4", {{"foshan-A"}}},
    {"RTX A4000", {{"west-B", "west-C"}}},
    {"CPU", {{"cq-A1"}, {"west-B", "west-C"}}},
    {"CPU-close-HT", {{"west-B", "west-C"}}},
    {"910B2x鲲鹏920", {{"gd-A1"}}}
};

// 当前时间字符串 (兼容跨平台)
std::string get_current_time_str() {
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

// 配置结构体
struct AppConfig {
    std::string authorization;
    std::string gpu_model = "RTX 4090";
    int check_interval_seconds = 30;
    std::string xizhi_url = "https://xizhi.qqoq.net/XZ92fff6ec8b293488bd67699b2d0a0faf.send";
    std::string config_file_path = "config.json";
    json raw_json;

    bool load(const std::string& path = "config.json") {
        config_file_path = path;
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        try {
            file >> raw_json;
            if (raw_json.contains("authorization") && raw_json["authorization"].is_string()) {
                authorization = raw_json["authorization"].get<std::string>();
            }
            if (raw_json.contains("gpu_model") && raw_json["gpu_model"].is_string()) {
                gpu_model = raw_json["gpu_model"].get<std::string>();
            } else if (raw_json.contains("gpu") && raw_json["gpu"].contains("model") && raw_json["gpu"]["model"].is_string()) {
                // 兼容旧版 config.json
                gpu_model = raw_json["gpu"]["model"].get<std::string>();
            }

            if (raw_json.contains("check_interval_seconds") && raw_json["check_interval_seconds"].is_number()) {
                check_interval_seconds = raw_json["check_interval_seconds"].get<int>();
            }

            if (raw_json.contains("xizhi_url") && raw_json["xizhi_url"].is_string()) {
                xizhi_url = raw_json["xizhi_url"].get<std::string>();
            } else {
                raw_json["xizhi_url"] = xizhi_url;
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool save() {
        raw_json["authorization"] = authorization;
        raw_json["gpu_model"] = gpu_model;
        raw_json["check_interval_seconds"] = check_interval_seconds;
        raw_json["xizhi_url"] = xizhi_url;

        std::ofstream file(config_file_path);
        if (!file.is_open()) return false;
        file << raw_json.dump(2);
        return true;
    }
};

// 手机号密码登录 AutoDL
bool login_with_password(AppConfig& config, const std::string& phone, const std::string& password) {
    std::cout << "[登录] 正在通过手机号密码登录 AutoDL..." << std::endl;
    std::string pwd_hash = utils::sha1(password);

    json login_req;
    login_req["phone"] = phone;
    login_req["password"] = pwd_hash;
    login_req["phone_area"] = "+86";

    auto resp = HttpClient::post_json("https://www.autodl.com/api/v1/new_login", login_req.dump());
    if (!resp.success) {
        std::cerr << "[错误] 登录请求失败: " << resp.error_message << " (状态码: " << resp.status_code << ")" << std::endl;
        return false;
    }

    try {
        auto j = json::parse(resp.body);
        if (j.value("code", "") != "Success") {
            std::cerr << "[错误] 登录失败: " << j.value("msg", "未知错误") << std::endl;
            return false;
        }
        std::string ticket = j["data"]["ticket"].get<std::string>();

        // 获取 token
        json passport_req;
        passport_req["ticket"] = ticket;
        auto passport_resp = HttpClient::post_json("https://www.autodl.com/api/v1/passport", passport_req.dump());
        if (!passport_resp.success) {
            std::cerr << "[错误] 获取 Token 失败: " << passport_resp.error_message << std::endl;
            return false;
        }

        auto pj = json::parse(passport_resp.body);
        if (pj.value("code", "") != "Success") {
            std::cerr << "[错误] 获取 Token 响应异常: " << pj.value("msg", "") << std::endl;
            return false;
        }

        config.authorization = pj["data"]["token"].get<std::string>();
        config.save();
        std::cout << "[成功] 登录成功并已自动更新到 config.json！" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[错误] 解析登录响应数据异常: " << e.what() << std::endl;
        return false;
    }
}

// 发送息知 (xizhi) 推送通知
bool send_xizhi_notification(const std::string& xizhi_url, const std::string& title, const std::string& content) {
    std::string full_url = xizhi_url;
    if (full_url.find('?') == std::string::npos) {
        full_url += "?title=" + HttpClient::url_encode(title) + "&content=" + HttpClient::url_encode(content);
    } else {
        full_url += "&title=" + HttpClient::url_encode(title) + "&content=" + HttpClient::url_encode(content);
    }

    std::cout << "[推送] 正在发送通知至息知平台..." << std::endl;
    auto resp = HttpClient::get(full_url);
    if (resp.success) {
        std::cout << "[推送] 息知响应: " << resp.body << std::endl;
        return true;
    } else {
        std::cerr << "[推送] 发送失败: " << resp.error_message << " (状态码: " << resp.status_code << ")" << std::endl;
        return false;
    }
}

// 检查某个区域的 GPU 状态
struct RegionGpuStatus {
    std::string region_raw;
    std::string region_name;
    bool has_available = false;
    int idle_num = 0;
    json raw_data;
};

RegionGpuStatus check_region_gpu(const AppConfig& config, const std::vector<std::string>& region_sign_list) {
    RegionGpuStatus status;
    json reg_json = region_sign_list;
    status.region_raw = reg_json.dump();

    auto it = REGION_NAMES.find(status.region_raw);
    if (it != REGION_NAMES.end()) {
        status.region_name = it->second;
    } else {
        status.region_name = status.region_raw;
    }

    json req_body;
    req_body["region_sign_list"] = region_sign_list;

    std::map<std::string, std::string> headers = {
        {"Authorization", config.authorization},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    auto resp = HttpClient::post_json("https://www.autodl.com/api/v1/machine/region/gpu_type",
                                      req_body.dump(), headers);

    if (!resp.success) {
        return status;
    }

    try {
        auto res_json = json::parse(resp.body);
        if (res_json.value("code", "") == "AuthorizeFailed") {
            std::cerr << "[警告] AutoDL Authorization 已失效或超时，请重新登录！" << std::endl;
            return status;
        }

        if (res_json.contains("data") && res_json["data"].is_array()) {
            for (const auto& item : res_json["data"]) {
                if (item.is_object() && item.contains(config.gpu_model)) {
                    auto gpu_obj = item[config.gpu_model];
                    status.raw_data = gpu_obj;

                    // 判断是否有可用机器
                    int idle = 0;
                    if (gpu_obj.contains("idle_num") && gpu_obj["idle_num"].is_number()) {
                        idle = gpu_obj["idle_num"].get<int>();
                    } else if (gpu_obj.contains("gpu_idle_num") && gpu_obj["gpu_idle_num"].is_number()) {
                        idle = gpu_obj["gpu_idle_num"].get<int>();
                    }

                    status.idle_num = idle;
                    if (idle > 0) {
                        status.has_available = true;
                    }
                    break;
                }
            }
        }
    } catch (...) {
        // JSON 解析忽略
    }

    return status;
}

int main() {
#ifdef _WIN32
    // 设置 Windows 控制台为 UTF-8 编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "====================================================\n";
    std::cout << "        AutoDL GPU 资源监控系统 (C++ 原生版)        \n";
    std::cout << "====================================================\n\n";

    AppConfig config;
    if (!config.load()) {
        std::cout << "[提示] 未检测到 config.json 或解析失败，正在初始化默认配置...\n";
        config.save();
    }

    // 检查并引导 authorization 凭证
    if (config.authorization.empty()) {
        std::cout << "[提示] 当前未配置 AutoDL Authorization。\n";
        std::cout << "请选择授权方式:\n";
        std::cout << "  1. 直接粘贴 Web 控制台的 Authorization Token (推荐)\n";
        std::cout << "  2. 输入 AutoDL 手机号和密码登录\n";
        std::cout << "请输入选项 (1 或 2): ";
        int choice = 1;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 2) {
            std::string phone, pwd;
            std::cout << "请输入手机号: ";
            std::cin >> phone;
            std::cout << "请输入密码: ";
            std::cin >> pwd;
            std::cin.ignore();
            if (!login_with_password(config, phone, pwd)) {
                std::cerr << "[退出] 登录未成功，程序退出。\n";
                return 1;
            }
        } else {
            std::cout << "请粘贴您的 Authorization Token: ";
            std::getline(std::cin, config.authorization);
            if (config.authorization.empty()) {
                std::cerr << "[错误] Authorization 不能为空！\n";
                return 1;
            }
            config.save();
            std::cout << "[成功] Token 已保存至 config.json！\n";
        }
    }

    // 检查所选的 GPU 型号
    auto it_regions = GPU_TO_REGIONS.find(config.gpu_model);
    if (it_regions == GPU_TO_REGIONS.end()) {
        std::cerr << "[警告] GPU 型号 '" << config.gpu_model << "' 未在内置预设区域列表中。\n";
        std::cerr << "默认将监控所有常用区域...\n";
    }

    std::cout << "\n----------------------------------------------------\n";
    std::cout << "监控目标 GPU : " << config.gpu_model << "\n";
    std::cout << "检测间隔     : " << config.check_interval_seconds << " 秒\n";
    std::cout << "息知推送接口 : " << config.xizhi_url << "\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << "[状态] 开始持续监控，按 Ctrl+C 可停止运行...\n\n";

    int round = 0;
    while (true) {
        round++;
        std::string current_time = get_current_time_str();
        std::cout << "[" << current_time << "] 第 " << round << " 次检测中..." << std::endl;

        std::vector<RegionGpuStatus> available_list;

        const auto& regions = (it_regions != GPU_TO_REGIONS.end()) 
                              ? it_regions->second 
                              : std::vector<std::vector<std::string>>{
                                  {"nm-B1", "nm-B2"}, {"cq-A1"}, {"west-B", "west-C"},
                                  {"bj-B1"}, {"beijing-A", "beijing-B", "beijing-D", "beijing-E"}
                              };

        for (const auto& reg_list : regions) {
            RegionGpuStatus st = check_region_gpu(config, reg_list);
            if (st.has_available) {
                available_list.push_back(st);
            }
        }

        if (!available_list.empty()) {
            std::cout << "\n====================================================" << std::endl;
            std::cout << "  🎉【发现可用 GPU 资源！】 型号: " << config.gpu_model << std::endl;
            std::cout << "====================================================" << std::endl;

            std::string content = "时间: " + current_time + "\n";
            content += "目标型号: " + config.gpu_model + "\n可用区域详情:\n";

            for (const auto& st : available_list) {
                std::string line = "  - " + st.region_name + " (空闲数量: " + std::to_string(st.idle_num) + " 台)";
                std::cout << line << std::endl;
                content += line + "\n";
            }
            content += "\n请尽快前往 AutoDL 控制台创建实例！";

            std::string title = "【AutoDL有卡提醒】" + config.gpu_model + " 发现可用机器";
            send_xizhi_notification(config.xizhi_url, title, content);

            std::cout << "\n[完成] 通知已推送，监控任务完成。" << std::endl;
            break;
        } else {
            std::cout << " -> 暂无可用空闲机器，" << config.check_interval_seconds << " 秒后进行下一次检测..." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(config.check_interval_seconds));
    }

    std::cout << "\n程序已退出，感谢使用。" << std::endl;
    return 0;
}
