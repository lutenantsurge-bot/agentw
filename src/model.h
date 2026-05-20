#pragma once
#include <string>
#include <vector>
#include <map>

// Forward declare to avoid circular includes
namespace agent_cpp {
    struct Message;
    struct ModelConfig;
    struct ToolCall;
}

class Model {
public:
    Model(const agent_cpp::ModelConfig& config);

    std::string chat(const std::vector<agent_cpp::Message>& messages);
    std::vector<agent_cpp::ToolCall> parse_tool_calls(const std::string& response);

private:
    agent_cpp::ModelConfig config_;
};
