#include "robot_tools.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace robot_tools {

std::string parse_robot_cmd(const std::string& response) {
    if (response.find("\"name\"") != std::string::npos &&
        response.find("\"rc_control\"") != std::string::npos) {
        std::size_t action_start = response.find("\"action\"");
        if (action_start != std::string::npos) {
            std::size_t colon     = response.find(':', action_start);
            std::size_t quote     = response.find('"', colon);
            std::size_t end_quote = response.find('"', quote + 1);
            if (quote != std::string::npos && end_quote != std::string::npos) {
                std::string action = response.substr(quote + 1, end_quote - quote - 1);
                std::size_t speed_start = response.find("\"speed\"");
                int speed = 160;
                if (speed_start != std::string::npos) {
                    std::size_t sc  = response.find(':', speed_start);
                    std::size_t sv  = response.find_first_of("0123456789", sc);
                    if (sv != std::string::npos) {
                        std::size_t se = response.find_first_not_of("0123456789", sv);
                        speed = std::stoi(response.substr(sv, se - sv));
                    }
                }
                std::stringstream ss;
                ss << "JSON:" << action << ":" << speed;
                return ss.str();
            }
        }
    }
    return parse_simple_robot_cmd(response);
}

RCControlTool::RCControlTool(SerialRobot& robot) : robot_(robot) {}

std::string RCControlTool::execute(const std::string& action, int speed, int angle) {
    std::string cmd;
    bool success = false;

    if      (action == "forward")        { cmd = "F" + std::to_string(speed); success = robot_.forward(speed); }
    else if (action == "backward")       { cmd = "B" + std::to_string(speed); success = robot_.backward(speed); }
    else if (action == "left")           { cmd = "L" + std::to_string(speed); success = robot_.left(speed); }
    else if (action == "right")          { cmd = "R" + std::to_string(speed); success = robot_.right(speed); }
    else if (action == "stop")           { cmd = "S";                          success = robot_.stop(); }
    else if (action == "emergency_stop") { cmd = "X";                          success = robot_.emergency_stop(); }
    else if (action == "arm")            { cmd = "A" + std::to_string(angle) + ":160"; success = robot_.arm(angle, speed); }
    else return "Unknown action: " + action;

    return success ? "Executed: " + cmd : "Failed to execute: " + cmd;
}

WebcamCaptureTool::WebcamCaptureTool(IPCam& cam) : cam_(cam) {}

std::string WebcamCaptureTool::capture(const std::string& save_as) {
    auto jpeg = cam_.grab_jpeg();
    if (jpeg.empty()) return "Failed to capture frame";

    std::ofstream out(save_as, std::ios::binary);
    if (!out) return "Failed to open file: " + save_as;

    out.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    out.close();

    std::stringstream ss;
    ss << "Captured " << jpeg.size() << " bytes to " << save_as;
    return ss.str();
}

} // namespace robot_tools
