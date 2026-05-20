#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>

// IPCamConfig struct
struct IPCamConfig {
    std::string host = "192.168.1.100";
    int port = 8080;
    bool enabled = false;
    int grab_interval_ms = 500;
};

// IPCam class - IP Webcam JPEG frame grabber (Android IP Webcam app)
class IPCam {
public:
    IPCam(const IPCamConfig& cfg);
    ~IPCam();

    std::vector<uint8_t> grab_jpeg();            // single frame, lowest latency
    void start_stream(std::function<void(const std::vector<uint8_t>&)> cb);
    void stop_stream();
    bool is_available();                         // probe connection, return true if reachable

private:
    int connect_socket();
    std::vector<uint8_t> http_get(const std::string& path);
    void stream_loop(std::function<void(const std::vector<uint8_t>&)> cb);

    IPCamConfig cfg_;
    std::atomic<bool> running_;
    std::thread stream_thread_;
};
