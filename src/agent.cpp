#include "agent.h"
#include "model.h"
#include "callback.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <functional>
#include <mutex>

namespace agent_cpp {

Agent::Agent(const ModelConfig& config)
    : config_(config)
    , model_(std::make_unique<Model>(config))
    , tool_dispatcher_(nullptr)
    , memory_file_("")
    , memory_mutex_()
{
    std::cout << "[Agent] Initializing...\n";
    std::cout << "  - Model:       " << config_.model << "\n";
    std::cout << "  - Base URL:    " << config_.base_url << "\n";
    std::cout << "  - Context:     " << config_.n_ctx << "\n";
    std::cout << "  - Temperature: " << config_.temperature << "\n";
    std::cout << "  - Tools count: " << count_brackets(config_.tools_json) << "\n";
}

void Agent::set_tool_dispatcher(const ToolDispatcher& dispatcher) {
    tool_dispatcher_ = dispatcher;
}

void Agent::enable_memory(const std::string& memory_file) {
    memory_file_ = memory_file;

    std::filesystem::path p(memory_file);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::cout << "[Agent] Memory enabled: " << memory_file << "\n";
}

std::string Agent::run(const std::string& user_message) {
    std::cout << "[Agent] Running with message: " << user_message << "\n";

    // Add user message — C++ struct init, not JSON syntax
    Message user_msg;
    user_msg.role    = "user";
    user_msg.content = user_message;
    messages_.push_back(user_msg);

    // Get model response
    std::string response = model_->chat(messages_);

    // Parse tool calls
    auto tool_calls = model_->parse_tool_calls(response);

    if (!tool_calls.empty()) {
        std::cout << "[Agent] Tool calls detected: " << tool_calls.size() << "\n";

        for (const auto& tc : tool_calls) {
            std::cout << "  - Tool: " << tc.name << "\n";
            std::cout << "    Args: " << tc.arguments_json << "\n";

            if (tool_dispatcher_) {
                std::string result = tool_dispatcher_(tc.name, tc.arguments);
                std::cout << "    Result: " << result << "\n";

                // Add tool response
                Message tool_msg;
                tool_msg.role    = "tool";
                tool_msg.name    = tc.name;
                tool_msg.content = result;
                messages_.push_back(tool_msg);
            }
        }

        // Follow-up response after tool calls
        response = model_->chat(messages_);
    }

    // Add assistant response
    Message assistant_msg;
    assistant_msg.role    = "assistant";
    assistant_msg.content = response;
    messages_.push_back(assistant_msg);

    return response;
}

size_t Agent::count_brackets(const std::string& json) {
    size_t count = 0;
    for (char c : json) {
        if (c == '{') count++;
    }
    return count;
}

} // namespace agent_cpp
