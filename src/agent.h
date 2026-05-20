#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <filesystem>
#include <mutex>

// Forward declare Model so unique_ptr compiles
class Model;

namespace agent_cpp {

struct ToolCall {
    std::string name;
    std::string arguments_json;
    std::map<std::string, std::string> arguments;
};

struct Message {
    std::string role;
    std::string content;
    std::string name;
};

struct ModelConfig {
    std::string base_url;
    std::string model;
    int n_ctx         = 32768;
    double temperature = 0.6;
    std::string tools_json;
};

using ToolDispatcher = std::function<
    std::string(const std::string&, const std::map<std::string, std::string>&)
>;

class Agent {
public:
    explicit Agent(const ModelConfig& config);

    void set_tool_dispatcher(const ToolDispatcher& dispatcher);
    void enable_memory(const std::string& memory_file);
    std::string run(const std::string& user_message);

private:
    ModelConfig            config_;
    std::unique_ptr<Model> model_;
    ToolDispatcher         tool_dispatcher_;
    std::vector<Message>   messages_;
    std::string            memory_file_;
    std::mutex             memory_mutex_;

    size_t count_brackets(const std::string& json);
};

} // namespace agent_cpp
