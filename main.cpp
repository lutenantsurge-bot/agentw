#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <memory>

#ifdef WITH_ROBOT
#include "serial_robot.h"
#include "robot_tools.h"
#endif

#ifdef WITH_IPCAM
#include "ipcam.h"
#include "robot_tools.h"
#endif

// ============================================================================
// Tool implementations
// ============================================================================
std::string file_search(const std::string& query, const std::string& path) {
    std::string results;
    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().filename().string().find(query) != std::string::npos)
                results += entry.path().string() + "\n";
        }
    } catch (const std::exception& e) {
        results = std::string("Error: ") + e.what();
    }
    return results.empty() ? "No results found." : results;
}

std::string read_file(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f) return "Error: cannot open file " + filepath;
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

std::string write_file(const std::string& filepath, const std::string& content) {
    std::ofstream f(filepath);
    if (!f) return "Error: cannot write to " + filepath;
    f << content;
    return "Written: " + filepath;
}

// ============================================================================
// Tool JSON schema — #ifdef cannot live inside a raw string literal
// Build it at runtime instead
// ============================================================================
std::string build_tools_json() {
    std::string tools = R"([
  {
    "type": "function",
    "function": {
      "name": "file_search",
      "description": "Search for files by name in a directory tree",
      "parameters": {
        "type": "object",
        "properties": {
          "query": { "type": "string", "description": "Filename or pattern to search for" },
          "path":  { "type": "string", "description": "Root directory to search from", "default": "." }
        },
        "required": ["query"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "read_file",
      "description": "Read the full contents of a file",
      "parameters": {
        "type": "object",
        "properties": {
          "filepath": { "type": "string", "description": "Path to the file" }
        },
        "required": ["filepath"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "write_file",
      "description": "Write content to a file",
      "parameters": {
        "type": "object",
        "properties": {
          "filepath": { "type": "string", "description": "Path to write to" },
          "content":  { "type": "string", "description": "Content to write" }
        },
        "required": ["filepath", "content"]
      }
    }
  }
)";

#ifdef WITH_ROBOT
    tools += R"(  ,{
    "type": "function",
    "function": {
      "name": "rc_control",
      "description": "Control RC car via serial robot",
      "parameters": {
        "type": "object",
        "properties": {
          "action": {
            "type": "string",
            "enum": ["forward","backward","left","right","stop","emergency_stop","arm"],
            "description": "Motor action"
          },
          "speed": { "type": "integer", "description": "Speed 0-255", "default": 160 },
          "angle": { "type": "integer", "description": "Servo angle 0-180", "default": 90 }
        },
        "required": ["action"]
      }
    }
  }
)";
#endif

#ifdef WITH_IPCAM
    tools += R"(  ,{
    "type": "function",
    "function": {
      "name": "webcam_capture",
      "description": "Capture image from IP Webcam",
      "parameters": {
        "type": "object",
        "properties": {
          "save_as": { "type": "string", "description": "Filename to save JPEG", "default": "frame.jpg" }
        }
      }
    }
  }
)";
#endif

    tools += "]";
    return tools;
}

// ============================================================================
// Global tool callbacks — set in main(), used in dispatch_tool
// ============================================================================
std::function<std::string(const std::string&, int, int)> g_rc_control;
std::function<std::string(const std::string&)>           g_webcam_capture;

// ============================================================================
// Tool dispatcher
// ============================================================================
std::string dispatch_tool(const std::string& name,
                           const std::map<std::string, std::string>& args) {
    if (name == "file_search") {
        std::string path = args.count("path") ? args.at("path") : ".";
        std::string query = args.count("query") ? args.at("query") : "";
        return file_search(query, path);
    }
    if (name == "read_file") {
        return read_file(args.count("filepath") ? args.at("filepath") : "");
    }
    if (name == "write_file") {
        return write_file(
            args.count("filepath") ? args.at("filepath") : "",
            args.count("content")  ? args.at("content")  : ""
        );
    }

#ifdef WITH_ROBOT
    if (name == "rc_control" && g_rc_control) {
        std::string action = args.count("action") ? args.at("action") : "stop";
        int speed = args.count("speed") ? std::stoi(args.at("speed")) : 160;
        int angle = args.count("angle") ? std::stoi(args.at("angle")) : 90;
        return g_rc_control(action, speed, angle);
    }
#endif

#ifdef WITH_IPCAM
    if (name == "webcam_capture" && g_webcam_capture) {
        std::string save_as = args.count("save_as") ? args.at("save_as") : "frame.jpg";
        return g_webcam_capture(save_as);
    }
#endif

    return "Unknown tool: " + name;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    bool    sbot      = false;
    std::string sbot_port = "/dev/ttyUSB0";
    int     sbot_baud = 115200;
    std::string cam_host  = "192.168.1.100";
    int     cam_port  = 8080;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "-sbot")                          sbot = true;
        else if (arg == "-sbot-port" && i+1 < argc)      sbot_port = argv[++i];
        else if (arg == "-sbot-baud" && i+1 < argc)      sbot_baud = std::stoi(argv[++i]);
        else if (arg == "-sbot-cam"  && i+1 < argc)      cam_host  = argv[++i];
        else if (arg == "-sbot-cam-port" && i+1 < argc)  cam_port  = std::stoi(argv[++i]);
    }

    std::cout << "llama-box + agent.cpp\n";
    std::cout << "======================\n\n";
    std::cout << "Robot:    " << (sbot ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Port:     " << sbot_port << " @" << sbot_baud << "\n";
    std::cout << "Webcam:   " << cam_host << ":" << cam_port << "\n";
    std::cout << "Whisper:  " << (getenv("WHISPER_ENABLED") ? "ENABLED" : "DISABLED") << "\n\n";

    std::cout << "Tools:\n";
    std::cout << "  file_search, read_file, write_file\n";
#ifdef WITH_ROBOT
    std::cout << "  rc_control\n";
#endif
#ifdef WITH_IPCAM
    std::cout << "  webcam_capture\n";
#endif
    std::cout << "\n";

    // Build tools JSON
    std::string tools_json = build_tools_json();

    // ── Robot init ───────────────────────────────────────────────────────────
#ifdef WITH_ROBOT
    std::unique_ptr<SerialRobot> robot;
    if (sbot) {
        SerialConfig scfg = {sbot_port, sbot_baud, true};
        robot = std::make_unique<SerialRobot>(scfg);
        if (!robot->open()) {
            std::cerr << "[warning] Robot serial failed, continuing without\n";
            robot.reset();
        } else {
            std::cout << "[robot] Serial ready at " << sbot_port << "\n";
        }
    }
    if (robot) {
        robot_tools::RCControlTool rc_tool(*robot);
        g_rc_control = [rc_tool](const std::string& action, int speed, int angle) mutable {
            return rc_tool.execute(action, speed, angle);
        };
    }
#endif

    // ── Camera init ──────────────────────────────────────────────────────────
#ifdef WITH_IPCAM
    std::unique_ptr<IPCam> cam;
    if (sbot) {
        IPCamConfig ccfg = {cam_host, cam_port, true};
        cam = std::make_unique<IPCam>(ccfg);
        if (!cam->is_available()) {
            std::cerr << "[warning] IP Webcam not reachable at "
                      << cam_host << ":" << cam_port << "\n";
        } else {
            std::cout << "[ipcam] Webcam ready at " << cam_host << ":" << cam_port << "\n";
        }
    }
    if (cam) {
        robot_tools::WebcamCaptureTool web_cam(*cam);
        g_webcam_capture = [web_cam](const std::string& save_as) mutable {
            return web_cam.capture(save_as);
        };
    }
#endif

    // ── Main loop ────────────────────────────────────────────────────────────
    std::cout << "Agent ready. Type your request (or 'exit'):\n> ";

    std::string user_input;
    while (std::getline(std::cin, user_input)) {
        if (user_input == "exit") break;
        if (user_input.empty()) { std::cout << "> "; continue; }

        std::cout << "\nProcessing: " << user_input << "\n";

#ifdef WITH_ROBOT
        if (sbot && robot) {
            auto cmd = robot_tools::parse_robot_cmd(user_input);
            if (!cmd.empty()) {
                std::cout << "[robot] Sending: " << cmd << "\n";
                robot->send(cmd);
            }
        }
#endif

        std::cout << "\n> ";
    }

    return 0;
}
