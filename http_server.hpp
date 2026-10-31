#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

// 极轻量级跨平台自包含 HTTP Server (单头文件实现，开放在 5178 端口)
class SimpleHttpServer {
public:
    using Handler = std::function<void(const std::string& method,
                                      const std::string& path,
                                      const std::string& body,
                                      std::string& resp_content_type,
                                      std::string& resp_body,
                                      int& resp_status)>;

private:
    int port_ = 5178;
    SOCKET server_socket_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    Handler handler_;

    static void init_network() {
#ifdef _WIN32
        static bool initialized = false;
        if (!initialized) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            initialized = true;
        }
#endif
    }

    void handle_client(SOCKET client_sock) {
        std::string raw_request;
        char buffer[4096];
        int bytes_read = 0;

        // 读取 HTTP 请求头和内容
        while ((bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes_read] = '\0';
            raw_request.append(buffer, bytes_read);
            if (raw_request.find("\r\n\r\n") != std::string::npos) {
                // 已读取完头部，检查 Content-Length
                size_t cl_pos = raw_request.find("Content-Length:");
                if (cl_pos == std::string::npos) cl_pos = raw_request.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t cl_end = raw_request.find("\r\n", cl_pos);
                    int content_len = std::stoi(raw_request.substr(cl_pos + 15, cl_end - (cl_pos + 15)));
                    size_t header_end = raw_request.find("\r\n\r\n") + 4;
                    while (raw_request.length() - header_end < static_cast<size_t>(content_len)) {
                        int more_bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                        if (more_bytes <= 0) break;
                        raw_request.append(buffer, more_bytes);
                    }
                }
                break;
            }
        }

        if (raw_request.empty()) {
            closesocket(client_sock);
            return;
        }

        // 解析请求行
        std::istringstream req_stream(raw_request);
        std::string method, path, http_ver;
        req_stream >> method >> path >> http_ver;

        size_t header_end = raw_request.find("\r\n\r\n");
        std::string body;
        if (header_end != std::string::npos && header_end + 4 < raw_request.length()) {
            body = raw_request.substr(header_end + 4);
        }

        // 调用业务路由处理
        std::string content_type = "text/html; charset=utf-8";
        std::string resp_body = "Not Found";
        int resp_status = 404;

        if (handler_) {
            handler_(method, path, body, content_type, resp_body, resp_status);
        }

        std::string status_msg = (resp_status == 200) ? "OK" : (resp_status == 404 ? "Not Found" : "Bad Request");
        std::ostringstream response;
        response << "HTTP/1.1 " << resp_status << " " << status_msg << "\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << resp_body.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        response << "Access-Control-Allow-Headers: Content-Type\r\n";
        response << "Connection: close\r\n\r\n";
        response << resp_body;

        std::string resp_str = response.str();
        send(client_sock, resp_str.c_str(), static_cast<int>(resp_str.length()), 0);
        closesocket(client_sock);
    }

public:
    SimpleHttpServer(int port = 5178) : port_(port) {
        init_network();
    }

    ~SimpleHttpServer() {
        stop();
    }

    void set_handler(Handler h) {
        handler_ = h;
    }

    bool start() {
        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket_ == INVALID_SOCKET) {
            std::cerr << "[WebUI] 创建 Socket 失败" << std::endl;
            return false;
        }

        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[WebUI] 绑定端口 " << port_ << " 失败 (端口可能已被占用)" << std::endl;
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        if (listen(server_socket_, 20) == SOCKET_ERROR) {
            std::cerr << "[WebUI] 监听端口失败" << std::endl;
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        running_ = true;
        server_thread_ = std::thread([this]() {
            while (running_) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                SOCKET client_sock = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
                if (client_sock == INVALID_SOCKET) {
                    if (!running_) break;
                    continue;
                }
                std::thread(&SimpleHttpServer::handle_client, this, client_sock).detach();
            }
        });

        std::cout << "[WebUI] 已成功启动！浏览器访问地址: http://localhost:" << port_ << std::endl;
        return true;
    }

    void stop() {
        if (running_) {
            running_ = false;
            if (server_socket_ != INVALID_SOCKET) {
                closesocket(server_socket_);
                server_socket_ = INVALID_SOCKET;
            }
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        }
    }
};
