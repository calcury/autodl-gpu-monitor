#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#endif

struct HttpResponse {
    int status_code = 0;
    std::string body;
    bool success = false;
    std::string error_message;
};

class HttpClient {
public:
    static std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (unsigned char c : value) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << '%' << std::setw(2) << std::uppercase << int(static_cast<unsigned char>(c));
            }
        }
        return escaped.str();
    }

#ifdef _WIN32
    // ==========================================
    // Windows 实现 (基于 Windows 系统内置 WinHTTP，免装任何三方库)
    // 编译命令: g++ -std=c++17 -O2 main.cpp -o autodl_monitor.exe -lws2_32 -lwinhttp
    // ==========================================
private:
    static std::wstring to_wide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
        return wstr;
    }

public:
    static HttpResponse request(const std::string& full_url,
                                const std::string& method = "GET",
                                const std::string& body = "",
                                const std::map<std::string, std::string>& headers = {}) {
        HttpResponse response;

        std::wstring w_url = to_wide(full_url);
        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwSchemeLength    = (DWORD)-1;
        urlComp.dwHostNameLength  = (DWORD)-1;
        urlComp.dwUrlPathLength   = (DWORD)-1;
        urlComp.dwExtraInfoLength = (DWORD)-1;

        if (!WinHttpCrackUrl(w_url.c_str(), (DWORD)w_url.length(), 0, &urlComp)) {
            response.error_message = "URL 解析失败";
            return response;
        }

        std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring path = (urlComp.dwUrlPathLength > 0) ? std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength) : L"/";
        if (urlComp.dwExtraInfoLength > 0) {
            path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        }

        INTERNET_PORT port = urlComp.nPort;
        bool is_https = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(L"AutoDL-Cpp-Monitor/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            response.error_message = "创建 HTTP Session 失败";
            return response;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            response.error_message = "连接服务器失败";
            return response;
        }

        DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
        std::wstring w_method = to_wide(method);
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, w_method.c_str(), path.c_str(),
                                               NULL, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            response.error_message = "创建 HTTP Request 失败";
            return response;
        }

        std::wstring w_headers;
        for (const auto& [k, v] : headers) {
            w_headers += to_wide(k) + L": " + to_wide(v) + L"\r\n";
        }

        BOOL bResults = WinHttpSendRequest(hRequest,
                                           w_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : w_headers.c_str(),
                                           w_headers.empty() ? 0 : (DWORD)w_headers.length(),
                                           (LPVOID)(body.empty() ? NULL : body.c_str()),
                                           (DWORD)body.length(),
                                           (DWORD)body.length(),
                                           0);

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (bResults) {
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
            response.status_code = (int)dwStatusCode;

            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1, 0);
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                    response.body.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);

            response.success = (response.status_code >= 200 && response.status_code < 400);
        } else {
            response.error_message = "请求发送或接收响应失败 (错误码: " + std::to_string(GetLastError()) + ")";
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

#else
    // ==========================================
    // Linux / POSIX 实现 (健壮型执行器，带错误诊断与防干扰)
    // ==========================================
private:
    static std::string escape_shell_arg(const std::string& arg) {
        std::string res = "'";
        for (char c : arg) {
            if (c == '\'') res += "'\\''";
            else res += c;
        }
        res += "'";
        return res;
    }

public:
    static HttpResponse request(const std::string& full_url,
                                const std::string& method = "GET",
                                const std::string& body = "",
                                const std::map<std::string, std::string>& headers = {}) {
        HttpResponse response;

        // 构造临时请求文件，彻底避免命令行 JSON 字符转义问题
        std::string tmp_file = "/tmp/autodl_req_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json";
        bool has_body = !body.empty() && (method == "POST" || method == "PUT");
        if (has_body) {
            std::ofstream ofs(tmp_file, std::ios::binary);
            if (ofs.is_open()) {
                ofs.write(body.data(), body.size());
                ofs.close();
            }
        }

        // 1. 构建 curl 命令
        // 参数说明:
        // -s: 静音进度条
        // -S: 出错时打印具体错误到 stderr
        // -k: 允许非受信任证书（防止微型系统缺少 ca-certificates）
        // --noproxy "*": 避免被本地残留的失效代理环境变量劫持
        // --connect-timeout 10: 10秒连接超时
        // -m 30: 30秒整体超时
        std::ostringstream cmd;
        cmd << "curl -s -S -k --noproxy '*' -w \"\\n%{http_code}\" --connect-timeout 10 -m 30 -X " << method;
        for (const auto& [k, v] : headers) {
            cmd << " -H " << escape_shell_arg(k + ": " + v);
        }
        if (has_body) {
            cmd << " --data-binary @" << tmp_file;
        }
        cmd << " " << escape_shell_arg(full_url) << " 2>&1"; // 合并输出方便定位真实原因

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (has_body) {
            std::remove(tmp_file.c_str());
        }

        if (!pipe) {
            response.error_message = "无法启动 curl 进程进行请求";
            return response;
        }

        std::string raw_output;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            raw_output += buffer;
        }
        int exit_code = pclose(pipe);

        // 如果命令未找到 (例如系统没有 curl)
        if (raw_output.find("not found") != std::string::npos || raw_output.find("command not found") != std::string::npos) {
            response.error_message = "系统未安装 curl 命令行，请执行 sudo apt-get install -y curl";
            response.status_code = 0;
            return response;
        }

        // 解析输出结果：最后一行是 HTTP 状态码
        size_t last_newline = raw_output.find_last_of("\r\n");
        while (last_newline != std::string::npos && (raw_output[last_newline] == '\r' || raw_output[last_newline] == '\n')) {
            raw_output.erase(last_newline);
            last_newline = raw_output.find_last_of("\r\n");
        }

        last_newline = raw_output.find_last_of("\r\n");
        std::string status_str;
        if (last_newline == std::string::npos) {
            status_str = raw_output;
            response.body = "";
        } else {
            status_str = raw_output.substr(last_newline + 1);
            response.body = raw_output.substr(0, last_newline);
        }

        try {
            response.status_code = std::stoi(status_str);
        } catch (...) {
            response.status_code = (exit_code == 0) ? 200 : 0;
            if (response.status_code == 0) {
                response.error_message = "连接异常: " + raw_output;
            }
        }

        response.success = (response.status_code >= 200 && response.status_code < 400);
        if (!response.success && response.error_message.empty()) {
            response.error_message = "HTTP 请求失败 (状态码: " + std::to_string(response.status_code) + ", 详情: " + raw_output + ")";
        }

        return response;
    }
#endif

    static HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) {
        return request(url, "GET", "", headers);
    }

    static HttpResponse post_json(const std::string& url, const std::string& json_body, const std::map<std::string, std::string>& extra_headers = {}) {
        auto headers = extra_headers;
        headers["Content-Type"] = "application/json";
        return request(url, "POST", json_body, headers);
    }
};
