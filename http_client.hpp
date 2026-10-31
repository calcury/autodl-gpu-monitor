#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
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
    // Windows 实现 (基于系统原生 WinHTTP，零外部依赖)
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
    // Linux / POSIX 实现 (基于标准 libcurl，编译只需 -lcurl)
    // ==========================================
private:
    static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string* s = static_cast<std::string*>(userp);
        s->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

public:
    static HttpResponse request(const std::string& full_url,
                                const std::string& method = "GET",
                                const std::string& body = "",
                                const std::map<std::string, std::string>& headers = {}) {
        HttpResponse response;
        CURL* curl = curl_easy_init();
        if (!curl) {
            response.error_message = "初始化 CURL 失败";
            return response;
        }

        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        // 设置请求方法与 Body
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
            }
        } else if (method != "GET") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
            }
        }

        // 设置 Headers
        struct curl_slist* chunk = NULL;
        for (const auto& [k, v] : headers) {
            std::string header_line = k + ": " + v;
            chunk = curl_slist_append(chunk, header_line.c_str());
        }
        if (chunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            response.status_code = static_cast<int>(http_code);
            response.success = (response.status_code >= 200 && response.status_code < 400);
        } else {
            response.error_message = curl_easy_strerror(res);
        }

        if (chunk) {
            curl_slist_free_all(chunk);
        }
        curl_easy_cleanup(curl);
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
