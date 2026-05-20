#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>

// Include robot modules if enabled
#ifdef WITH_ROBOT
#include "serial_robot.h"
#include "robot_tools.h"
#endif

#ifdef WITH_IPCAM
#include "ipcam.h"
#include "robot_tools.h"
#endif

// Tool implementations
std::string file_search(const std::string& query, const std::string& path) {
    std::string results;
    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().filename().string().find(query) != std::string::npos) {
                results += entry.path().string() + "\n";
            }
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

// Tool definitions (JSON Schema)
const std::string TOOLS_JSON = R"([
  {
    "type": "function",
    "function": {
      "name": "file_search",
      "description": "Search for files by name in a directory tree",
      "parameters": {
        "type": "object",
        "properties": {
          "query": {
            "type": "string",
            "description": "Filename or pattern to search for"
          },
          "path": {
            "type": "string",
            "description": "Root directory to search from",
            "default": "."
          }
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
          "filepath": {
            "type": "string",
            "description": "Absolute or relative path to the file"
          }
        },
        "required": ["filepath"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "write_file",
      "description": "Write content to a file, creating it if it does not exist",
      "parameters": {
        "type": "object",
        "properties": {
          "filepath": {
            "type": "string",
            "description": "Path to write to"
          },
          "content": {
            "type": "string",
            "description": "Content to write"
          }
        },
        "required": ["filepath", "content"]
      }
    }
  }
#ifdef WITH_ROBOT
  ,
  {
    "type": "function",
    "function": {
      "name": "rc_control",
      "description": "Control L298N RC Car via serial robot",
      "parameters": {
        "type": "object",
        "properties": {
          "action": {
            "type": "string",
            "enum": ["forward", "backward", "left", "right", "stop", "emergency_stop"],
            "description": "Motor action to execute"
          },
          "speed": {
            "type": "integer",
            "description": "Motor speed (0-255)",
            "default": 160
          },
          "angle": {
            "type": "integer",
            "description": "Servo arm angle (0-180), used for 'arm' action",
            "default": 90
          }
        },
        "required": ["action"]
      }
    }
  }
#endif
#ifdef WITH_IPCAM
  ,
  {
    "type": "function",
    "function": {
      "name": "webcam_capture",
      "description": "Capture image from IP Webcam",
      "parameters": {
        "type": "object",
        "properties": {
          "save_as": {
            "type": "string",
            "description": "Filename to save JPEG frame",
            "default": "frame.jpg"
          }
        }
      }
    }
  }
#endif
])";

// Tool dispatcher
std::string dispatch_tool(const std::string& name,
                           const std::map<std::string, std::string>& args) {
    if (name == "file_search") {
        std::string path = args.count("path") ? args.at("path") : ".";
        return file_search(args.at("query"), path);
    }
    if (name == "read_file")  return read_file(args.at("filepath"));
    if (name == "write_file") return write_file(args.at("filepath"), args.at("content"));
    
#ifdef WITH_ROBOT
    if (name == "rc_control") {
        // We'll need to set this up in main()
        extern std::string g_rc_control(const std::string& action, int speed, int angle);
        std::string action = args.count("action") ? args.at("action") : "forward";
        int speed = args.count("speed") ? std::stoi(args.at("speed")) : 160;
        int angle = args.count("angle") ? std::stoi(args.at("angle")) : 90;
        return g_rc_control(action, speed, angle);
    }
#endif
    
#ifdef WITH_IPCAM
    if (name == "webcam_capture") {
        extern std::string g_webcam_capture(const std::string& save_as);
        std::string save_as = args.count("save_as") ? args.at("save_as") : "frame.jpg";
        return g_webcam_capture(save_as);
    }
#endif
    
    return "Unknown tool: " + name;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool sbot = false;
    std::string sbot_port = "/dev/ttyUSB0";
    int sbot_baud = 115200;
    std::string cam_host = "192.168.1.100";
    int cam_port = 8080;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-sbot") {
            sbot = true;
        } else if (arg == "-sbot-port" && i + 1 < argc) {
            sbot_port = argv[++i];
        } else if (arg == "-sbot-baud" && i + 1 < argc) {
            sbot_baud = std::stoi(argv[++i]);
        } else if (arg == "-sbot-cam" && i + 1 < argc) {
            cam_host = argv[++i];
        } else if (arg == "-sbot-cam-port" && i + 1 < argc) {
            cam_port = std::stoi(argv[++i]);
        }
    }
    
    std::cout << "llama-box + agent.cpp + robot/camera\n";
    std::cout << "========================================\n\n";
    std::cout << "This combines:\n";
    std::cout << "  - llama-box: Inference server with multimodal support\n";
    std::cout << "  - agent.cpp: C++ agent loop with tool calling\n";
    std::cout << "  - Robot modules: Serial control + IP Webcam\n\n";
    std::cout << "Architecture:\n";
    std::cout << "  GGUF model (+ mmproj) -> llama-box server :8080\n";
    std::cout << "  -> agent.cpp client (tool defs, loop, memory)\n";
    std::cout << "  -> main.cpp (glue, tool implementations)\n\n";
    
    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Robot mode: " << (sbot ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  Serial port: " << sbot_port << "\n";
    std::cout << "  Baud rate: " << sbot_baud << "\n";
    std::cout << "  IP Webcam: " << cam_host << ":" << cam_port << "\n";
    std::cout << "  Voice input: " << (getenv("WHISPER_ENABLED") ? "ENABLED" : "DISABLED") << "\n\n";
    
    std::cout << "Tools available:\n";
    std::cout << "  - file_search: Search files by name\n";
    std::cout << "  - read_file: Read file contents\n";
    std::cout << "  - write_file: Write to file\n";
#ifdef WITH_ROBOT
    std::cout << "  - rc_control: Control RC car via serial\n";
#endif
#ifdef WITH_IPCAM
    std::cout << "  - webcam_capture: Capture from IP Webcam\n";
#endif
    
    // Initialize robot and camera if enabled
#ifdef WITH_ROBOT
    SerialConfig scfg = {sbot_port, sbot_baud, true};
    std::unique_ptr<SerialRobot> robot;
    if (sbot) {
        robot = std::make_unique<SerialRobot>(scfg);
        if (!robot->open()) {
            std::cerr << "[warning] Robot serial failed to open, continuing without robot\n";
            robot.reset();
        } else {
            std::cout << "[robot] Serial initialized at " << sbot_port << " @" << sbot_baud << "\n";
        }
    }
#endif
    
#ifdef WITH_IPCAM
    IPCamConfig ccfg = {cam_host, cam_port, true};
    std::unique_ptr<IPCam> cam;
    if (sbot) {
        cam = std::make_unique<IPCam>(ccfg);
        if (!cam->is_available()) {
            std::cerr << "[warning] IP Webcam not reachable at " << cam_host << ":" << cam_port << "\n";
        } else {
            std::cout << "[ipcam] IP Webcam reachable at " << cam_host << ":" << cam_port << "\n";
        }
    }
#endif
    
    // Global tool callbacks (for dispatch_tool)
    std::function<std::string(const std::string&, int, int)> g_rc_control;
    std::function<std::string(const std::string&)> g_webcam_capture;
    
#ifdef WITH_ROBOT
    if (sbot && robot) {
        robot_tools::RCControlTool rc_tool(*robot);
        g_rc_control = [&rc_tool](const std::string& action, int speed, int angle) {
            return rc_tool.execute(action, speed, angle);
        };
    }
#endif
    
#ifdef WITH_IPCAM
    if (sbot && cam) {
        robot_tools::WebcamCaptureTool web_cam(*cam);
        g_webcam_capture = [&web_cam](const std::string& save_as) {
            return web_cam.capture(save_as);
        };
    }
#endif
    
    std::cout << "\nAgent ready. Type your request (or 'exit'):\n> ";
    
    std::string user_input;
    while (std::getline(std::cin, user_input)) {
        if (user_input == "exit") break;
        std::cout << "\nProcessing: " << user_input << "\n";
        
        // In real implementation, this would call agent.cpp
        // For demo, just echo the command
        if (sbot) {
            auto cmd = robot_tools::parse_robot_cmd(user_input);
            if (!cmd.empty() && robot) {
                std::cout << "[robot] Sending command: " << cmd << "\n";
                robot->send(cmd);
            }
        }
        
        std::cout << "\n> ";
    }
    
    return 0;
}
