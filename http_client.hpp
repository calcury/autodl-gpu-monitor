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
#define WIN32_LEAN_AND_MEAN
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

            response.success = (response.status_code >= 200 && response.status_code < 400 && !response.body.empty());
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
    // Linux / POSIX 实现 (基于安全文件重定向，彻底杜绝换行转义及空输入问题)
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

        // 构造高精度纳秒级唯一临时文件名
        auto now_ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::string req_file = "/tmp/autodl_req_" + std::to_string(now_ns) + ".json";
        std::string resp_file = "/tmp/autodl_resp_" + std::to_string(now_ns) + ".json";

        bool has_body = !body.empty() && (method == "POST" || method == "PUT");
        if (has_body) {
            std::ofstream ofs(req_file, std::ios::binary);
            if (ofs.is_open()) {
                ofs.write(body.data(), body.size());
                ofs.close();
            }
        }

        // 关键设计: 使用 -o 将完整响应体写入文件，使用 -w %{http_code} 只把纯状态码输出到 stdout
        std::ostringstream cmd;
        cmd << "curl -s -S -k --noproxy '*' -o " << escape_shell_arg(resp_file)
            << " -w '%{http_code}' --connect-timeout 10 -m 30 -X " << method;
        for (const auto& [k, v] : headers) {
            cmd << " -H " << escape_shell_arg(k + ": " + v);
        }
        if (has_body) {
            cmd << " --data-binary @" << escape_shell_arg(req_file);
        }
        cmd << " " << escape_shell_arg(full_url) << " 2>&1";

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (has_body) {
            std::remove(req_file.c_str());
        }

        if (!pipe) {
            response.error_message = "无法启动 curl 进程进行请求";
            return response;
        }

        std::string stdout_str;
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            stdout_str += buffer;
        }
        int exit_code = pclose(pipe);

        // 从独立响应文件中读取完整响应体
        std::ifstream ifs(resp_file, std::ios::binary);
        if (ifs.is_open()) {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            response.body = ss.str();
            ifs.close();
            std::remove(resp_file.c_str());
        }

        // 解析状态码 (提取最后出现的 3 位连续数字)
        try {
            std::string code_digits;
            for (int i = static_cast<int>(stdout_str.length()) - 1; i >= 0; --i) {
                if (std::isdigit(static_cast<unsigned char>(stdout_str[i]))) {
                    code_digits = stdout_str[i] + code_digits;
                    if (code_digits.length() == 3) break;
                } else if (!code_digits.empty()) {
                    break;
                }
            }
            if (!code_digits.empty()) {
                response.status_code = std::stoi(code_digits);
            } else {
                response.status_code = (exit_code == 0) ? 200 : 0;
            }
        } catch (...) {
            response.status_code = (exit_code == 0) ? 200 : 0;
        }

        response.success = (response.status_code >= 200 && response.status_code < 400 && !response.body.empty());
        if (!response.success && response.error_message.empty()) {
            if (response.body.empty()) {
                response.error_message = "服务器返回内容为空 (HTTP状态码: " + std::to_string(response.status_code) + ", 输出: " + stdout_str + ")";
            } else {
                response.error_message = "HTTP 请求失败 (状态码: " + std::to_string(response.status_code) + ")";
            }
        }

        return response;
    }
#endif

    static HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) {
        return request(url, "GET", "", headers);
    }

    static HttpResponse post_json(const std::string& url, const std::string& json_body, const std::map<std::string, std::string>& extra_headers = {}) {
        auto headers = extra_headers;
        headers["Content-Type"] = "application/json;charset=UTF-8";
        return request(url, "POST", json_body, headers);
    }
};
