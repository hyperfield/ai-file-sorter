#ifndef LLMCLIENT_HPP
#define LLMCLIENT_HPP

#include "ILLMClient.hpp"
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
    /**
     * @brief Image input is sent as standard chat-completions image_url content.
     *
     * Whether the configured model actually accepts images is the caller's decision
     * (see CustomApiEndpoint::supports_vision); nothing is probed here.
     */
    bool supports_image_input() const override { return true; }
    std::string complete_prompt_with_image(const std::string& prompt,
                                           const std::string& image_path,
                                           const ImageCompletionOptions& options) override;

private:
    std::string api_key;
    /** @brief POSTs a chat request and returns the raw response body; HTTP errors throw. */
    std::string send_api_request_raw(std::string json_payload);
    std::string send_api_request(std::string json_payload);
    std::string make_payload(const std::string &file_name,
                             const std::string &file_path,
                                const FileType file_type,
                                const std::string& consistency_context);
    std::string make_generic_payload(const std::string& system_prompt,
                                     const std::string& user_prompt,
                                     int max_tokens) const;
    std::string effective_model() const;
    /**
     * @brief Resolve the final /chat/completions URL from the base URL or default.
     */
    std::string resolve_api_url() const;
    bool prompt_logging_enabled{false};
    std::string last_prompt;
    std::string model;
    std::string base_url;
};

#endif
