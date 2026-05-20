#pragma once

#include <functional>
#include <string>

// Callbacks for agent state changes
using ProgressCallback = std::function<void(const std::string&)>;
using ToolCallback = std::function<void(const std::string&, const std::string&)>;
using MessageCallback = std::function<void(const std::string&)>;
