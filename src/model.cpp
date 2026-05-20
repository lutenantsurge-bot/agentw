#include "agent.h"
#include "model.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <curl/curl.h>

using namespace agent_cpp;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Model::Model(const ModelConfig& config) : config_(config) {
    std::cout << "[Model] Initializing:\n";
    std::cout << "  base_url:    " << config_.base_url    << "\n";
    std::cout << "  model:       " << config_.model       << "\n";
    std::cout << "  n_ctx:       " << config_.n_ctx       << "\n";
    std::cout << "  temperature: " << config_.temperature << "\n";
}

std::string Model::chat(const std::vector<Message>& messages) {
    std::ostringstream payload;
    payload << "{";
    payload << "\"model\":\"" << config_.model << "\",";
    payload << "\"temperature\":" << config_.temperature << ",";
    payload << "\"max_tokens\":2048,";
    payload << "\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) payload << ",";
        payload << "{\"role\":\"" << messages[i].role
                << "\",\"content\":\"" << messages[i].content << "\"}";
    }
    payload << "]}";

    std::string body = payload.str();
    std::string response_str;

    CURL* curl = curl_easy_init();
    if (!curl) { std::cerr << "[Model] curl init failed\n"; return ""; }

    std::string url = config_.base_url + "/chat/completions";
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       120L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[Model] curl error: " << curl_easy_strerror(res) << "\n";
        return "";
    }

    // Extract content field
    std::string key = "\"content\":\"";
    size_t pos = response_str.find(key);
    if (pos == std::string::npos) {
        std::cerr << "[Model] No content in response: " << response_str << "\n";
        return "";
    }
    pos += key.size();
    size_t end = response_str.find("\"", pos);
    while (end != std::string::npos && response_str[end-1] == '\\')
        end = response_str.find("\"", end + 1);

    return response_str.substr(pos, end - pos);
}

std::vector<ToolCall> Model::parse_tool_calls(const std::string& response) {
    std::vector<ToolCall> tool_calls;

    // Custom raw string delimiter avoids )" ambiguity
    std::regex tool_regex(R"DELIM("name"\s*:\s*"([^"]+)")DELIM");
    std::regex args_regex(R"DELIM("arguments"\s*:\s*(\{[^}]+\}))DELIM");
    std::regex arg_pair (R"DELIM("([^"]+)"\s*:\s*"([^"]+)")DELIM");

    std::sregex_iterator iter(response.begin(), response.end(), tool_regex);
    std::sregex_iterator end;

    while (iter != end) {
        ToolCall tc;
        tc.name = (*iter)[1].str();

        std::string sub = response.substr(iter->position());
        std::smatch am;
        if (std::regex_search(sub, am, args_regex)) {
            tc.arguments_json = am[1].str();
            std::string args  = tc.arguments_json;
            std::sregex_iterator ai(args.begin(), args.end(), arg_pair);
            std::sregex_iterator ae;
            while (ai != ae) {
                tc.arguments[(*ai)[1].str()] = (*ai)[2].str();
                ++ai;
            }
        }
        tool_calls.push_back(tc);
        ++iter;
    }
    return tool_calls;
}
