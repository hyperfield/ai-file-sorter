#ifndef LLMCLIENT_HPP
#define LLMCLIENT_HPP

#include "ILLMClient.hpp"
#include "ChatCompletionTransport.hpp"
#include <Types.hpp>
#include <string>

class LLMClient : public ILLMClient {
public:
    /**
     * @brief Create an OpenAI-compatible client, optionally targeting a custom base URL.
     */
    LLMClient(std::string api_key, std::string model, std::string base_url = std::string());
    ~LLMClient() override;
    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type,
                                const std::string& consistency_context) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;
    void set_prompt_logging_enabled(bool enabled) override;

private:
#ifdef AI_FILE_SORTER_TEST_BUILD
    friend struct APICompletionTestAccess;
#endif
    ChatCompletionTransport transport_;
    std::string send_api_request(std::string json_payload);
    std::string make_payload(const std::string &file_name,
                             const std::string &file_path,
                                const FileType file_type,
                                const std::string& consistency_context);
    std::string make_completion_payload(
                                     const std::string& user_prompt,
                                     int max_tokens) const;
    std::string effective_model() const;
    bool prompt_logging_enabled{false};
    std::string last_prompt;
    std::string model;
};

#endif
