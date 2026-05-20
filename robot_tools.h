#pragma once
#include "serial_robot.h"
#include "ipcam.h"
#include <string>
#include <memory>

namespace robot_tools {

// Parse robot command from agent response
std::string parse_robot_cmd(const std::string& response);

// Robot control tool
class RCControlTool {
public:
    RCControlTool(SerialRobot& robot);
    std::string execute(const std::string& action, int speed, int angle = 0);
    
private:
    SerialRobot& robot_;
};

// IP Webcam capture tool
class WebcamCaptureTool {
public:
    WebcamCaptureTool(IPCam& cam);
    std::string capture(const std::string& save_as);
    
private:
    IPCam& cam_;
};

// Parse simple keywords for robot commands
inline std::string parse_simple_robot_cmd(const std::string& response) {
    std::string lower = response;
    for (auto& c : lower) c = tolower(c);
    
    if (lower.find("forward") != std::string::npos) return "F160";
    if (lower.find("backward") != std::string::npos || lower.find("back") != std::string::npos) return "B160";
    if (lower.find("left") != std::string::npos) return "L160";
    if (lower.find("right") != std::string::npos) return "R160";
    if (lower.find("stop") != std::string::npos) return "S";
    if (lower.find("emergency") != std::string::npos || lower.find("x") != std::string::npos) return "X";
    
    return "";
}

} // namespace robot_tools
