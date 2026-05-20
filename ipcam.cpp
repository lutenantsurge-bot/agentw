#include "ipcam.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <algorithm>

IPCam::IPCam(const IPCamConfig& cfg)
    : cfg_(cfg), running_(false), stream_thread_() {}

IPCam::~IPCam() {
    stop_stream();
}

int IPCam::connect_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg_.port);
    
    struct hostent* hp = gethostbyname(cfg_.host.c_str());
    if (!hp) {
        close(fd);
        return -1;
    }
    
    memcpy(&addr.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

std::vector<uint8_t> IPCam::http_get(const std::string& path) {
    int fd = connect_socket();
    if (fd < 0) return {};

    std::string request = "GET " + path + " HTTP/1.0\r\nHost: " + cfg_.host + ":" + std::to_string(cfg_.port) + "\r\n\r\n";
    
    send(fd, request.c_str(), request.size(), 0);
    
    std::vector<uint8_t> buffer;
    char recv_buf[4096];
    
    while (true) {
        ssize_t n = recv(fd, recv_buf, sizeof(recv_buf), 0);
        if (n <= 0) break;
        buffer.insert(buffer.end(), recv_buf, recv_buf + n);
    }
    
    close(fd);
    return buffer;
}

std::vector<uint8_t> IPCam::grab_jpeg() {
    auto data = http_get("/shot.jpg");
    
    // Strip HTTP headers - find "\r\n\r\n"
    auto it = std::find(data.begin(), data.end(), '\r');
    while (it != data.end()) {
        auto next = std::find(it + 1, data.end(), '\n');
        if (next != data.end() && next + 1 != data.end() && *(next + 1) == '\r') {
            auto next2 = std::find(next + 2, data.end(), '\n');
            if (next2 != data.end()) {
                return std::vector<uint8_t>(next2 + 1, data.end());
            }
        }
        it = next + 1;
    }
    
    return data;
}

void IPCam::start_stream(std::function<void(const std::vector<uint8_t>&)> cb) {
    running_ = true;
    stream_thread_ = std::thread(&IPCam::stream_loop, this, cb);
}

void IPCam::stop_stream() {
    running_ = false;
    if (stream_thread_.joinable()) {
        stream_thread_.join();
    }
}

void IPCam::stream_loop(std::function<void(const std::vector<uint8_t>&)> cb) {
    while (running_) {
        int fd = connect_socket();
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.grab_interval_ms));
            continue;
        }

        std::string request = "GET /video HTTP/1.0\r\nHost: " + cfg_.host + ":" + std::to_string(cfg_.port) + "\r\n\r\n";
        send(fd, request.c_str(), request.size(), 0);

        std::vector<uint8_t> buffer;
        char recv_buf[4096];

        while (running_) {
            ssize_t n = recv(fd, recv_buf, sizeof(recv_buf), 0);
            if (n <= 0) break;
            buffer.insert(buffer.end(), recv_buf, recv_buf + n);

            // Look for JPEG boundaries: 0xFF 0xD8 (start) to 0xFF 0xD9 (end)
            size_t start = 0, end = 0;
            bool in_jpeg = false;

            for (size_t i = 0; i < buffer.size(); ++i) {
                if (!in_jpeg && buffer[i] == 0xFF && (i + 1 < buffer.size()) && buffer[i + 1] == 0xD8) {
                    start = i;
                    in_jpeg = true;
                    ++i;
                } else if (in_jpeg && buffer[i] == 0xFF && (i + 1 < buffer.size()) && buffer[i + 1] == 0xD9) {
                    end = i + 2;
                    in_jpeg = false;
                    std::vector<uint8_t> jpeg(buffer.begin() + start, buffer.begin() + end);
                    cb(jpeg);
                    buffer.erase(buffer.begin(), buffer.begin() + end);
                    break;
                }
            }
        }

        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.grab_interval_ms));
    }
}

bool IPCam::is_available() {
    int fd = connect_socket();
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return false;
}
