#pragma once
#include <string>
#include <vector>

// SerialConfig struct
struct SerialConfig {
    std::string port = "/dev/ttyUSB0";
    int baud = 115200;
    bool enabled = false;
};

// SerialRobot class - SimpleSerial USB control for Arduino/ESP32
// Protocol (text over USB serial, newline terminated):
// F<speed>  - Forward, speed 0–255
// B<speed>  - Backward
// L<speed>  - Turn left (motors opposite)
// R<speed>  - Turn right
// S         - Stop
// X         - Emergency stop
// A<n>:<deg> - Arm servo n to degree (0–180)
class SerialRobot {
public:
    SerialRobot(const SerialConfig& cfg);
    ~SerialRobot();

    bool open();
    void close();
    bool send(const std::string& cmd);           // raw send, appends \n
    std::string recv(int timeout_ms = 200);      // poll for timeout_ms, return as string

    // Movement commands
    bool forward(int speed);
    bool backward(int speed);
    bool left(int speed);
    bool right(int speed);
    bool stop();
    bool emergency_stop();
    bool arm(int n, int deg);

private:
    SerialConfig cfg_;
    int fd_;                                     // file descriptor, -1 if closed
};
