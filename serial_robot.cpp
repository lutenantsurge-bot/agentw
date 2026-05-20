#include "serial_robot.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <iostream>

SerialRobot::SerialRobot(const SerialConfig& cfg)
    : cfg_(cfg), fd_(-1) {}

SerialRobot::~SerialRobot() {
    close();
}

bool SerialRobot::open() {
    fd_ = open(cfg_.port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "[serial] Failed to open " << cfg_.port << std::endl;
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "[serial] tcgetattr failed" << std::endl;
        close(fd_);
        return false;
    }

    // 8N1 config (8 bits, no parity, 1 stop bit)
    cfsetospeed(&tty, B115200);  // default 115200
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    tty.c_cflag &= ~PARENB;                       // no parity
    tty.c_cflag &= ~CSTOPB;                       // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                      // no flow control

    // Raw input
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    // Non-blocking read (VMIN=0, VTIME=1 = 0.1s timeout)
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "[serial] tcsetattr failed" << std::endl;
        close(fd_);
        return false;
    }

    std::cout << "[serial] Opened " << cfg_.port << " @ " << cfg_.baud << std::endl;
    return true;
}

void SerialRobot::close() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool SerialRobot::send(const std::string& cmd) {
    if (fd_ < 0) return false;
    std::string full_cmd = cmd + "\n";
    ssize_t written = write(fd_, full_cmd.c_str(), full_cmd.size());
    return written == (ssize_t)(full_cmd.size());
}

std::string SerialRobot::recv(int timeout_ms) {
    if (fd_ < 0) return "";

    char buf[256];
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) return "";

    ssize_t n = read(fd_, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return std::string(buf, n);
    }
    return "";
}

// Movement commands
bool SerialRobot::forward(int speed) {
    std::string cmd = "F" + std::to_string(speed);
    return send(cmd);
}

bool SerialRobot::backward(int speed) {
    std::string cmd = "B" + std::to_string(speed);
    return send(cmd);
}

bool SerialRobot::left(int speed) {
    std::string cmd = "L" + std::to_string(speed);
    return send(cmd);
}

bool SerialRobot::right(int speed) {
    std::string cmd = "R" + std::to_string(speed);
    return send(cmd);
}

bool SerialRobot::stop() {
    return send("S");
}

bool SerialRobot::emergency_stop() {
    return send("X");
}

bool SerialRobot::arm(int n, int deg) {
    std::string cmd = "A" + std::to_string(n) + ":" + std::to_string(deg);
    return send(cmd);
}
