#include "LLMClient.hpp"

#include "LocalLLMPromptBuilder.hpp"
#include "Logger.hpp"

#include <iostream>
#include <sstream>
#include <utility>

namespace {
std::string escape_json(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                out += c;
        }
    }
    return out;
}

} // namespace

LLMClient::LLMClient(std::string api_key, std::string model, std::string base_url)
    : transport_(std::move(api_key), std::move(base_url)), model(std::move(model))
{}


LLMClient::~LLMClient() = default;


void LLMClient::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled = enabled;
}


std::string LLMClient::send_api_request(std::string json_payload) {
    return transport_.complete(json_payload);
}

std::string LLMClient::effective_model() const
{
    return model.empty() ? "gpt-4o-mini" : model;
}

std::string LLMClient::categorize_file(const std::string& file_name,
                                       const std::string& file_path,
                                       FileType file_type,
                                       const std::string& consistency_context)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting remote categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting remote categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string json_payload = make_payload(file_name, file_path, file_type, consistency_context);

    if (prompt_logging_enabled && !last_prompt.empty()) {
        std::cout << "\n[DEV][PROMPT] Categorization request\n" << last_prompt << "\n";
    }

    std::string category = send_api_request(json_payload);

    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Categorization reply\n" << category << "\n";
    }

    return category;
}


std::string LLMClient::make_payload(const std::string& file_name,
                                    const std::string& file_path,
                                    const FileType file_type,
                                    const std::string& consistency_context)
{
    const auto prompt = LocalLLMPromptBuilder::build_user_prompt(
        file_name, file_path, file_type, consistency_context);
    last_prompt = prompt;
    const auto system_prompt = LocalLLMPromptBuilder::build_system_prompt(
        file_path, file_type, consistency_context);
    const std::string escaped_prompt = escape_json(prompt);
    const std::string escaped_system = escape_json(system_prompt);

    std::ostringstream payload;
    payload << "{\n"
            << "    \"model\": \"" << escape_json(effective_model()) << "\",\n"
            << "    \"messages\": [\n"
            << "        {\"role\": \"system\", \"content\": \"" << escaped_system << "\"},\n"
            << "        {\"role\": \"user\", \"content\": \"" << escaped_prompt << "\"}\n"
            << "    ]\n"
            << "}";

    return payload.str();
}

std::string LLMClient::make_completion_payload(
                                            const std::string& user_prompt,
                                            int max_tokens) const
{
    const std::string system_prompt(LLMPrompt::kCompletionSystemPrompt);
    std::ostringstream payload;
    payload << "{\"model\": \"" << escape_json(effective_model()) << "\",";
    payload << "\"messages\": [";
    payload << "{\"role\": \"system\", \"content\": \""
            << escape_json(system_prompt) << "\"},";
    payload << "{\"role\": \"user\", \"content\": \""
            << escape_json(user_prompt) << "\"}]";
    if (max_tokens > 0) {
        payload << ",\"max_tokens\": " << max_tokens;
    }
    payload << "}";
    return payload.str();
}

std::string LLMClient::complete_prompt(const std::string& prompt,
                                       int max_tokens)
{
    if (prompt_logging_enabled) {
        std::cout << "\n[DEV][PROMPT] Completion request\n"
                  << prompt << "\n";
    }
    std::string json_payload = make_completion_payload(prompt, max_tokens);
    std::string response = send_api_request(json_payload);
    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Completion reply\n" << response << "\n";
    }
    return response;
}
